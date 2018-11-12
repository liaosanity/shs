#pragma once

#include <stdint.h>
#include <map>
#include <vector>
#include <set>
#include <string>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "types.h"

#include "core/shs_conn.h"
#include "core/shs_conn_pool.h"
#include "core/shs_epoll.h"
#include "core/shs_event.h"
#include "service/service.h"

namespace shs 
{

class Config;
class TimedEventWatcher;
class PipedEventWatcher;
class ResultWrapper;
class ModuleWrapper;
class InvokeTimer;

class Framework 
{
public:
    Framework(Config* cfg);
    virtual ~Framework();

    void AddService(ServicePtr service);
    void RemoveService(const std::string& type);
    void RemoveServiceExcept(const std::string& type);

    void ContinueService();
    void PauseService();
    void StopService();
    void BindService();
    void RegisterService();

    void Invoke(const std::string& module_name,
        const std::string& method_name,
        const std::map<std::string, std::string>& request_params,
        int32_t timeout_ms,
        const InvokeCompleteHandler& invoke_complete_handler,
        boost::shared_ptr<InvokeParams> invoke_params);

    bool InitInMaster();
    bool InitInWorker(event_base_t* base, event_timer_t* timer, 
        conn_pool_t* conn_pool);
    bool InitInMonitor(event_base_t* base, event_timer_t* timer, 
        conn_pool_t* conn_pool);

    void Start();
    void Stop();
    void Close();

    bool IsValidMethod(const std::string& module, 
        const std::string& method) const;
    std::map<std::string, std::set<std::string>> GetModulesAndMethods() const;

    event_base_t *event_base() { return event_base_; }
    Config* config() { return cfg_; }

private:
    bool InitModule(const std::string& name, const std::string& path, 
        const std::string& conf);
    boost::shared_ptr<ModuleWrapper> FindModule(
        const std::string& module_name) const;
    bool WatchInvokeComplete();
    bool AddInvokeTimer(const InvokeCompleteHandler& complete_handler,
        uint64_t id, int32_t timeout_ms, bool ignore_stats);
    void InvokeComplete(uint64_t id, bool ignore_stats, 
        const InvokeResult& result);
    void HandleInvokeComplete();
    void HandleInvokeTimeout(uint64_t id, bool ignore_stats);

protected:
    Config *cfg_;
    event_base_t *event_base_;
    event_timer_t *event_timer_;
    conn_pool_t *conn_pool_;

    std::vector<ServicePtr> services_;
    std::map<std::string, boost::shared_ptr<ModuleWrapper> > modules_;
    std::map<uint64_t, boost::shared_ptr<InvokeTimer> > timers_;
    std::vector<boost::shared_ptr<ResultWrapper> > results_;
    boost::scoped_ptr<PipedEventWatcher> result_watcher_;
    boost::scoped_ptr<TimedEventWatcher> waiting_stop_;
    boost::mutex results_mtx_;

    uint64_t invoke_id_;
    bool exiting;
    bool ev_has_been_added;
};

} // namespace shs
