#include "module_impl.h"

#include <gflags/gflags.h>

#include "log/logging.h"

#include "task.h"
#include "module.h"
#include "worker.h"
#include "stats.h"

namespace shs 
{

using namespace std;
using namespace boost;

const int kMaxThreadNum = 256;

ModuleImpl::ModuleImpl(Module *module)
    : module_(module)
{
}

ModuleImpl::~ModuleImpl()
{
    Close();
}

void ModuleImpl::Stop()
{
    for (auto iter = workers_.begin(); iter != workers_.end(); ++iter)
    {
        iter->second->Stop();
    }
}

bool ModuleImpl::InitInMaster(const string& name, const string& conf) 
{
    name_ = name;
    conf_ = conf;

    try 
    {
        if (!module_->InitInMaster(conf)) 
        {
            return false;
        }
    } 
    catch (const std::exception& e) 
    {
        fprintf(stderr, "ModuleImpl::InitInMaster Exception, ex=%s\n", 
            e.what());

        return false;
    }

    return true;
}

bool ModuleImpl::InitInWorker(int32_t connection_n) 
{
    int thread_num = 1;

    try 
    {
        if (!module_->InitInWorker(&thread_num)) 
        {
            SLOG(ERROR) << "ModuleImpl::InitInWorker() Failed!";

            return false;
        }
    } 
    catch (const std::exception& e) 
    {
        SLOG(ERROR) << "ModuleImpl::InitInWorker Exception Init module failed!" 
            << e.what();

        return false;
    }

    if (!SpawnWorkerThreads(thread_num, connection_n)) 
    {
        SLOG(ERROR) << "ModuleImpl::InitInWorker \
            SpawnWorkerThreads module failed!"; 

        return false;
    }

    try
    {
        for (int i = 0; i < thread_num; ++i) 
        {
            if (!module_->InitWithBase()) 
            {
                SLOG(ERROR) << "InitWithBase() Failed!";

                return false;
            }
        }
    }
    catch (const std::exception& e) 
    {
        SLOG(ERROR) << "ModuleImpl::InitInWorker Exception \
            InitWithBase module failed. " << e.what();

        return false;
    }

    return true;
}

bool ModuleImpl::SpawnWorkerThreads(int thread_num, int32_t connection_n) 
{
    boost::unique_lock<boost::shared_mutex> wlock(workers_rwmtx_);

    if (thread_num <= 0) 
    {
        thread_num = 1;
    } 
    else if (thread_num > kMaxThreadNum) 
    {
        thread_num = kMaxThreadNum;
    }

    for (int i = 0; i < thread_num; ++i) 
    {
        boost::shared_ptr<Worker> worker(new Worker(false));
        if (!worker->Init(connection_n)) 
        {
            goto failed;
        }

        if (!worker->Start()) 
        {
            goto failed;
        }

        auto r = workers_.insert(make_pair(worker->id(), worker));
        if (!r.second) 
        {
            goto failed;
        }

        worker_pos_.push_back(r.first);
    }

    return true;

failed:
    workers_.clear();
    worker_pos_.clear();

    return false;
}

void ModuleImpl::Close() 
{
    boost::unique_lock<boost::shared_mutex> wlock(workers_rwmtx_);
    workers_.clear();
    worker_pos_.clear();
}

void ModuleImpl::Register(const string& method_name,
    const InvokeHandler& method) 
{
    boost::unique_lock<boost::shared_mutex> wlock(methods_rwmtx_);
    methods_[method_name] = boost::shared_ptr<InvokeHandler>(
        new InvokeHandler(method));
}

void ModuleImpl::Invoke(const string& method_name,
    const map<string, string>& request_params,
    const InvokeCompleteHandler& complete_handler,
    boost::shared_ptr<InvokeParams> invoke_params) 
{
    boost::shared_ptr<InvokeHandler> method;
    {
        boost::shared_lock<boost::shared_mutex> rlock(methods_rwmtx_);
        auto it = methods_.begin();
        if ((it = methods_.find(method_name)) != methods_.end())
        {
            method = it->second;
        }
        else if (default_method_)
        {
            method = default_method_;
        }
        else
        {
            SLOG(ERROR) << "Can not find method '" << method_name << "'";

            InvokeResult ir;
            ir.set_ec(ErrorCode::E_INVALID_METHOD);
            ir.set_msg("Invalid method name: " + method_name);
            complete_handler(ir);

            return;
        }
    }

    boost::shared_ptr<Task> task;
    task.reset(new Task(method, request_params, 
        complete_handler, invoke_params));

    if (!DispatchTask(task)) 
    {
        SLOG(ERROR) << "Dispatch task failed";

        ProcessStats::AddErrorRequest();

        InvokeResult ir;
        ir.set_ec(ErrorCode::E_SERVICE_BUSY);
        ir.set_msg("Service temporary busy.");
        complete_handler(ir);
    }
}

bool ModuleImpl::DispatchTask(boost::shared_ptr<Task> task) 
{
    boost::shared_lock<boost::shared_mutex> rlock(workers_rwmtx_);

    size_t tried = 0;
    while (tried++ < worker_pos_.size()) 
    {
        size_t idx = (last_worker_++) % worker_pos_.size();
        WorkerMap::iterator it = worker_pos_[idx];
        if (it->second->AddTask(task)) 
        {
            return true;
        }
    }

    return false;
}

event_base_t *ModuleImpl::event_base() 
{
    boost::shared_lock<boost::shared_mutex> rlock(workers_rwmtx_);

    WorkerMap::iterator it = workers_.find(boost::this_thread::get_id());
    if (it == workers_.end()) 
    {
        return NULL;
    }

    return it->second->event_base();
}

conn_pool_t *ModuleImpl::conn_pool() 
{
    boost::shared_lock<boost::shared_mutex> rlock(workers_rwmtx_);

    WorkerMap::iterator it = workers_.find(boost::this_thread::get_id());
    if (it == workers_.end()) 
    {
        return NULL;
    }

    return it->second->conn_pool();
}

event_timer_t *ModuleImpl::event_timer() 
{
    boost::shared_lock<boost::shared_mutex> rlock(workers_rwmtx_);

    WorkerMap::iterator it = workers_.find(boost::this_thread::get_id());
    if (it == workers_.end()) 
    {
        return NULL;
    }

    return it->second->event_timer();
}

std::set<std::string> ModuleImpl::GetAllMethods() const
{
    std::set<std::string> v;
    for (auto& method : methods_)
    {
        v.insert(method.first);
    }

    return v;
}

bool ModuleImpl::IsValidMethod(const std::string& method) const
{
    if (methods_.find(method) != methods_.end())
    {
        return true;
    }

    return false;
}

} // namespace shs
