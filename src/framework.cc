#include "framework.h"

#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <tr1/functional>
#include <boost/algorithm/string/predicate.hpp>
#include <gflags/gflags.h>

#include "http/invoke_timer.h"
#include "http/invoke_params.h"
#include "log/logging.h"
#include "core/shs_event_timer.h"

#include "event_watcher.h"
#include "result_wrapper.h"
#include "module_wrapper.h"
#include "config.h"
#include "stats.h"
#include "process_cycle.h"

extern bool g_running;

namespace shs 
{

using namespace std;
using namespace boost;

Framework::Framework(Config *cfg)
    : cfg_(cfg)
    , event_base_(NULL)
    , event_timer_(NULL)
    , conn_pool_(NULL)
    , invoke_id_(0)
    , exiting(false)
    , ev_has_been_added(true)
{
}

Framework::~Framework()
{
    if (result_watcher_)
    {
        result_watcher_->Close();
    }
}

void Framework::AddService(ServicePtr service)
{
    services_.push_back(service);
}

void Framework::RemoveService(const std::string& type)
{
    auto it = services_.begin();
    for (; it != services_.end(); ++it)
    {
        if ((*it)->GetType() == type)
        {
            break;   
        }
    }

    if (it != services_.end())
    {
        services_.erase(it);
    }
}

void Framework::RemoveServiceExcept(const std::string& type)
{
    ServicePtr save;
    for (auto service : services_)
    {
        if (service->GetType() == type)
        {
            save = service;
            break;
        }
    }

    services_.clear();
    if (save)
    {
        services_.push_back(save);
    }
}

void Framework::PauseService()
{
    for (auto service : services_)
    {
        service->Pause();
    }
}

void Framework::StopService()
{    
    for (auto service : services_)
    {
        service->Stop();
    }
}

void Framework::ContinueService()
{    
    for (auto service : services_)
    {
        service->Continue();
    }
}

void Framework::BindService()
{    
    for (auto service : services_)
    {
        service->ListenExpectAddress();
    }
}

void Framework::RegisterService()
{    
    for (auto service : services_)
    {
        service->RegisterEvent(event_base_, event_timer_, conn_pool_);
    }
}

bool Framework::InitInMaster() 
{
    bool init_success = true;

    if (0 == modules_.size()) 
    {
        DIR *dir = opendir(cfg_->mod_path().c_str());
        if (!dir) 
        {
            fprintf(stderr, "Open module path failed, path=%s, err=%s\n",
                cfg_->mod_path().c_str(), strerror(errno));

            return false;
        }

        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) 
        {
            if (0 == strcmp(entry->d_name, ".") 
                || 0 == strcmp(entry->d_name, ".."))
            {
                continue;
            }

            string filename(entry->d_name);
            string name;
            if (!boost::ends_with(filename, ".so")) 
            {
                continue;
            }

            if (boost::starts_with(filename, "lib")) 
            {
                name = filename.substr(3, filename.length() - 6);
            }
            else 
            {
                name = filename.substr(0, filename.length() - 3);
            }

            string path = cfg_->mod_path() + "/" + filename;
            string conf = cfg_->mod_conf() + "/" + name + ".conf";
            if (!InitModule(name, path, conf)) 
            {
                init_success = false;
            }
        }

        closedir(dir);
    } 
    else 
    {
        for (auto iter = modules_.begin(); iter != modules_.end(); ++iter) 
        {
            if (!(*iter).second->InitInMaster()) 
            {
                init_success = false;
            }
        }
    }

    return init_success;
}

bool Framework::InitModule(const std::string& name,
    const std::string& path, const std::string& conf) 
{
    boost::shared_ptr<ModuleWrapper> m(new ModuleWrapper());
    if (!m->InitInMaster(name, path, conf)) 
    {
        fprintf(stderr, "InitModule failed, module=%s, path=%s, conf=%s\n", 
            name.c_str(), path.c_str(), conf.c_str());

        return false;
    }

    modules_[name] = m;

    return true;
}

bool Framework::InitInWorker(event_base_t *base, 
    event_timer_t *timer, conn_pool_t* conn_pool) 
{
    event_base_ = base;
    event_timer_ = timer;
    conn_pool_ = conn_pool;

    if (!WatchInvokeComplete()) 
    {
        SLOG(ERROR) << "InitInWorker WatchInvokeComplete Failed!";

        return false;
    }

    for (auto iter = modules_.begin(); iter != modules_.end(); ++iter) 
    {
        if (!iter->second->InitInWorker(cfg_->connection_n()))
        {
            SLOG(ERROR) << "InitInWorker Failed!";

            return false;
        }
    }

    return true;
}

bool Framework::InitInMonitor(event_base_t *base, 
    event_timer_t *timer, conn_pool_t* conn_pool)
{
    event_base_ = base;
    event_timer_ = timer;
    conn_pool_ = conn_pool;

    return true;
}

bool Framework::WatchInvokeComplete() 
{
    result_watcher_.reset(new PipedEventWatcher(event_base_,
        std::tr1::bind(&Framework::HandleInvokeComplete, this)));

    return result_watcher_->Init();
}

void Framework::Start() 
{
    Close();
}

void Framework::Stop() 
{
    bool wait = false;

    if (!exiting)
    {
        exiting = true;

        waiting_stop_.reset(new TimedEventWatcher(event_base(), event_timer_, 
            10 * 1000, std::tr1::bind(&Framework::Stop, this)));
        if (waiting_stop_->Init())
        {
            return;
        }
    }

    for (auto& module : modules_)
    {
        module.second->Stop();
    }

    if (!results_.empty())
    {
        wait = true;
    }

    if (timers_.size() > 0)
    {
        wait = true;
    }

    if (wait)
    {
        waiting_stop_.reset(new TimedEventWatcher(event_base_, event_timer_, 
            30 * 1000, std::tr1::bind(&Framework::Stop, this)));
        if (waiting_stop_->Init())
        {
            return;
        }
    }
    else
    {
        modules_.clear();
    }

    g_running = false;
}

void Framework::Close() 
{
}

boost::shared_ptr<ModuleWrapper> Framework::FindModule(
    const std::string &module_name) const
{
    auto it = modules_.find(module_name);
    if (it == modules_.end()) 
    {
        return boost::shared_ptr<ModuleWrapper>();
    } 
    else 
    {
        return it->second;
    }
}

void Framework::Invoke(
    const std::string& module_name, const std::string& method_name, 
    const std::map<std::string, std::string>& request_params,
    int32_t timeout_ms, const InvokeCompleteHandler& complete_handler,
    boost::shared_ptr<InvokeParams> invoke_params)
{
    uint64_t invoke_id = invoke_id_++;

    bool ignore_stats = false;
    if (0 == strcmp(method_name.c_str(), "Status"))
    {
        ignore_stats = true;
    }

    if (!ignore_stats)
    {
        ProcessStats::AddRequest();
    }

    boost::shared_ptr<ModuleWrapper> module = FindModule(module_name);
    if (!module) 
    {
        SLOG(ERROR) << __FILE__ << ":" << __LINE__ << "\tInvalid module name";

        InvokeResult ir;
        ir.set_ec(ErrorCode::E_INVALID_MODULE);
        ir.set_msg("Invalid module name");
        complete_handler(ir);

        return;
    }

    if (!AddInvokeTimer(complete_handler, invoke_id, timeout_ms, ignore_stats))
    {
        ProcessStats::AddErrorRequest();

        SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
            << "\tAdd timer failed for id=" << invoke_id;

        InvokeResult ir;
        ir.set_ec(ErrorCode::E_INTERNAL);
        ir.set_msg("Invoke Initialize timer failed");
        complete_handler(ir);

        return;
    }
    timers_[invoke_id]->set_module(module_name);

    invoke_params->set_enqueue_time(Timestamp::Now().MicroSecondsSinceEpoch());
    invoke_params->set_timer_queue_size(timers_.size());
    invoke_params->set_timer(timers_[invoke_id]);
 
    ProcessStats::SetQueueSize(timers_.size());

#ifndef USE_SO_REUSEPORT
    if (cfg_->num_processes() > 1 && ev_has_been_added) 
    {
        int max, curr, zero;
        get_queue_status(NULL, &max, &curr, &zero);
        if (zero > 0 || curr >= max 
            || (cfg_->queue_hwm() > 0 && curr >= cfg_->queue_hwm()))
        {
            PauseService();
            ev_has_been_added = false;
            SLOG(TRACE) << "PauseService, zero(" 
                << zero << ") > 0 || curr(" << curr
                << ") >= max(" << max << ") || (queue_hwm(" 
                << cfg_->queue_hwm() << ") > 0 && curr(" 
                << curr << ") >= queue_hwm(" << cfg_->queue_hwm() << "))";
        }
    }
#endif

    if (module)
    {
        module->Invoke(method_name, request_params,
            std::tr1::bind(&Framework::InvokeComplete, this, 
            invoke_id, ignore_stats, std::tr1::placeholders::_1), invoke_params);
    }
}

void Framework::InvokeComplete(uint64_t id, bool ignore_stats,
    const InvokeResult& result)
{
    bool need_notify = false;
    {
        boost::mutex::scoped_lock lock(results_mtx_);
        if (results_.empty()) 
        {
            need_notify = true;
        }
        results_.push_back(boost::shared_ptr<ResultWrapper>(
            new ResultWrapper(id, ignore_stats, result)));
    }

    if (need_notify) 
    {
        result_watcher_->Notify();
    }
}

bool Framework::AddInvokeTimer(
    const InvokeCompleteHandler& complete_handler,
    uint64_t id, int32_t timeout_ms, bool ignore_stats)
{
    boost::shared_ptr<InvokeTimer> timer(new InvokeTimer(event_base_, 
        event_timer_, timeout_ms, complete_handler, 
        std::tr1::bind(&Framework::HandleInvokeTimeout,
        this, id, ignore_stats)));

    if (!timers_.insert(make_pair(id, timer)).second) 
    {
        return false;
    }

    update_queue_length(1);

    return true;
}

void Framework::HandleInvokeTimeout(uint64_t id, bool ignore_stats)
{
    auto it = timers_.find(id);

    boost::shared_ptr<InvokeTimer> timer = it->second;
    timers_.erase(it);
    update_queue_length(-1);

    InvokeResult ir;
    ir.set_ec(ErrorCode::E_INVOKE_TIMEOUT);
    ir.set_msg("Invoke timed out");
    timer->Complete(ir);

    if (!ignore_stats)
    {
        ProcessStats::AddTimeoutRequest();
        ProcessStats::AddInvokeElapsedTime(timer->ElapsedTime());
        ProcessStats::AddElapsedTimeInQueue(timer->ElapsedTimeInQueue());
    }

    ProcessStats::SetQueueSize(timers_.size());

#ifndef USE_SO_REUSEPORT
    if (cfg_->num_processes() > 1 && !ev_has_been_added)
    {
        int min, curr;
        get_queue_status(&min, NULL, &curr, NULL);
        if (curr == 0 || curr <= min 
            || (cfg_->queue_hwm() > 0 && cfg_->queue_lwm() > 0 
            && curr <= cfg_->queue_lwm()))
        {
            ContinueService();
            ev_has_been_added = true;
            LOG(TRACE) << "HandleInvokeTimeout -> ContinueService, curr(" 
                << curr << ") == 0" << " || curr(" << curr << ") <= min(" 
                << min << ") || (queue_hwm(" << cfg_->queue_hwm()
                << ") > 0 && queue_lwm(" << cfg_->queue_lwm() << ") > 0 && curr(" 
                << curr << ") <= queue_lwm(" << cfg_->queue_lwm() << "))";
        }
    }
#endif
}

void Framework::HandleInvokeComplete() 
{
    std::vector<boost::shared_ptr<ResultWrapper> > results;
    {
        boost::mutex::scoped_lock lock(results_mtx_);
        results.swap(results_);
    }
 
    for (auto res : results)
    {
        auto tit = timers_.find(res->id());
        if (tit == timers_.end()) 
        {
            continue;
        }

        boost::shared_ptr<InvokeTimer> timer = tit->second;
        timers_.erase(tit);
        update_queue_length(-1);

        timer->Cancel();
        timer->Complete(res->result());

        if (!res->ignore_stats())
        {
            ProcessStats::AddInvokeElapsedTime(timer->ElapsedTime());
            ProcessStats::AddElapsedTimeInQueue(timer->ElapsedTimeInQueue());
        }
    }

    ProcessStats::SetQueueSize(timers_.size());

#ifndef USE_SO_REUSEPORT
    if (cfg_->num_processes() > 1 && !ev_has_been_added)
    {
        int min, curr;
        get_queue_status(&min, NULL, &curr, NULL);
        if (curr == 0 || curr <= min 
            || (cfg_->queue_hwm() > 0 && cfg_->queue_lwm() > 0 
            && curr <= cfg_->queue_lwm()))
        {
            ContinueService();
            ev_has_been_added = true;
            LOG(TRACE) << "HandleInvokeComplete -> ContinueService, curr(" 
                << curr << ") == 0" << " || curr(" << curr << ") <= min(" 
                << min << ") || (queue_hwm(" << cfg_->queue_hwm()
                << ") > 0 && queue_lwm(" << cfg_->queue_lwm() << ") > 0 && curr(" 
                << curr << ") <= queue_lwm(" << cfg_->queue_lwm() << "))";
        }
    }
#endif
}

std::map<std::string, std::set<std::string>> 
    Framework::GetModulesAndMethods() const
{
    std::map<std::string, std::set<std::string>> result;
    for (auto& module : modules_)
    {
        auto v = module.second->GetAllMethods();
        result[module.first] = v;
    }

    return result;
}

bool Framework::IsValidMethod(const std::string& module, 
    const std::string& method) const
{
    auto it = modules_.find(module);
    if (it == modules_.end())
    {
        return false;
    }

    return it->second->IsValidMethod(method);
}

} // namespace shs
