#include "module.h"

#include <map>
#include <string>
#include <tr1/functional>

#include "module_impl.h"
#include "process_cycle.h"

namespace shs 
{

using namespace std;
using namespace std::tr1::placeholders;

Module::Module()
    : impl_(new ModuleImpl(this))
{
    Register("Status", tr1::bind(&Module::Status, this, 
        tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3));
}

Module::~Module() 
{
    delete impl_;
    impl_ = NULL;
}

bool Module::InitInMaster(const std::string& conf) 
{
    return true;
}

bool Module::InitInWorker(int *thread_num)
{
    return true;
}

bool Module::InitWithBase()
{
    return true;
}

void Module::Status(const map<string, string>& params, 
    const InvokeCompleteHandler& cb, 
    boost::shared_ptr<InvokeParams> invoke_params)
{
    shs::InvokeResult result;
    cb(result);
}

void Module::Stop()
{
    impl_->Stop();
}

void Module::Register(const std::string& method, 
    const InvokeHandler& handler) 
{
    impl_->Register(method, handler);
}

const std::string& Module::conf() const 
{
    return impl_->conf();
}

const std::string& Module::name() const 
{
    return impl_->name();
}

Module::ProcessType Module::GetProcessType() const
{
    if (g_process == SHS_PROCESS_MASTER)
    {
        return kMaster;
    }
    else if (g_process == SHS_PROCESS_MONITOR)
    {
        return kMonitor;
    }

    return kWorker;
}

event_base_t *Module::event_base() 
{
    return impl_->event_base();
}

conn_pool_t *Module::conn_pool() 
{
    return impl_->conn_pool();
}

event_timer_t* Module::event_timer()
{
    return impl_->event_timer();
}

} // namespace shs
