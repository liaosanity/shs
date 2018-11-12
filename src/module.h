#pragma once

#include <string>

#include "types.h"

#include "core/shs_epoll.h"
#include "core/shs_conn_pool.h"
#include "http/invoke_params.h"

#define EXPORT_MODULE(m) \
extern "C" void *CreateModuleInstance() \
{ \
    return new m(); \
} \
extern "C" void DestroyModuleInstance(void *ptr) \
{ \
    m *p = (m*)ptr; \
    delete p; \
} // EXPORT_MODULE

namespace shs 
{

class ModuleImpl;

class Module
{
    friend class ModuleImpl;

public:
    enum ProcessType
    {
        kMaster,
        kWorker,
        kMonitor
    };

    Module();
    virtual ~Module();

    virtual bool InitInMaster(const std::string& conf);
    virtual bool InitInWorker(int* thread_num);
    virtual bool InitWithBase();

    void Stop();
    void Status(const std::map<std::string, std::string>& params, 
        const InvokeCompleteHandler& cb, 
        boost::shared_ptr<InvokeParams> invoke_params);

    void Register(const std::string& method, const InvokeHandler& handler);

    const std::string& name() const;
    const std::string& conf() const;
    ProcessType GetProcessType() const;

    event_base_t* event_base();
    conn_pool_t* conn_pool();
    event_timer_t* event_timer();

private:
    ModuleImpl* impl_;
};

} // namespace shs
