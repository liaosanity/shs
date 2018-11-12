#pragma once

#include <map>
#include <set>
#include <vector>
#include <string>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "core/shs_epoll.h"

#include "types.h"
#include "module.h"
#include "event_watcher.h"

namespace shs 
{

class Task;
class Worker;

class ModuleImpl 
{
    typedef std::map<boost::thread::id, boost::shared_ptr<Worker> > WorkerMap;

public:
    ModuleImpl(Module * module);
    ~ModuleImpl();

    bool InitInMaster(const std::string& name, const std::string& conf);
    bool InitInWorker(int32_t connection_n);

    void Register(const std::string& method_name, const InvokeHandler& method);

    void Invoke(const std::string& method_name,
        const std::map<std::string, std::string>& request_params,
        const InvokeCompleteHandler& complete_handler,
        boost::shared_ptr<InvokeParams> invoke_params);
    void Stop();

    const std::string& name() const { return name_; }
    const std::string& conf() const { return conf_; }

    event_base_t *event_base();
    conn_pool_t *conn_pool();
    event_timer_t *event_timer();

    static ModuleImpl *Get(Module *module) 
    {
        return module->impl_;
    }

    static ModuleImpl *Get(boost::shared_ptr<Module> module) 
    {
        return Get(module.get());
    }

    std::set<std::string> GetAllMethods() const;
    bool IsValidMethod(const std::string& method) const;

private:
    bool SpawnWorkerThreads(int thread_num, int32_t connection_n);
    void Close();
    bool DispatchTask(boost::shared_ptr<Task> task);

    std::string name_;
    std::string conf_;
    Module *module_;
    boost::shared_mutex methods_rwmtx_;
    std::map<std::string, boost::shared_ptr<InvokeHandler> > methods_;
    boost::shared_ptr<InvokeHandler> default_method_;
    boost::shared_mutex workers_rwmtx_;
    WorkerMap workers_;
    std::vector<WorkerMap::iterator> worker_pos_;
    uint64_t last_worker_;
};

} // namespace shs
