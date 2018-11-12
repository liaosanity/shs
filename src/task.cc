#include "task.h"

#include <gflags/gflags.h>

#include "http/invoke_timer.h"
#include "http/invoke_params.h"
#include "http/http_invoke_params.h"
#include "comm/timestamp.h"

DEFINE_bool(drop_expired_task, true, "drop the expired task");

namespace shs 
{

using namespace std;
using namespace boost;

Task::Task(boost::shared_ptr<InvokeHandler> method,
    const std::map<std::string, std::string>& request_params,
    const InvokeCompleteHandler& complete_handler,
    boost::shared_ptr<InvokeParams> invoke_params)
    : method_(method)
    , request_params_(request_params)
    , complete_handler_(complete_handler)
    , invoke_params_(invoke_params)
{
}

void Task::Run() const 
{
    if (IsExpired())
    {
        InvokeResult ir;
        ir.set_ec(ErrorCode::E_REQUEST_EXPIRE);
        ir.set_msg("Invoke failed: already pass upstream expire time");
        complete_handler_(ir);

        return;
    }

    if (invoke_params_)
    {
        auto guard = invoke_params_->get_timer();
        if (guard)
        {
            guard->set_dequeue_time();
        }
        else
        {
            if (FLAGS_drop_expired_task)
            {
                return;
            }
        }
    }

    try 
    {
        if (method_)
        {
            invoke_params_->set_dequeue_time(
                Timestamp::Now().MicroSecondsSinceEpoch());
            (*method_)(request_params_, complete_handler_, invoke_params_);
        }
        else
        {
            InvokeResult ir;
            ir.set_ec(ErrorCode::E_INVOKE_FAILED);
            ir.set_msg("Invoke failed: invalid method");
            complete_handler_(ir);
        }
    } 
    catch (const std::exception& e) 
    {
        InvokeResult ir;
        ir.set_ec(ErrorCode::E_INVOKE_FAILED);
        ir.set_msg(string("Invoke failed: ") + e.what());
        complete_handler_(ir);
    }
}

void Task::SetTaskSize(size_t size)
{
    if (invoke_params_)
    {
        invoke_params_->set_task_queue_size(size);
    }
}

bool Task::IsExpired() const
{
    if (!invoke_params_)
    {
        return false;
    }

    auto http_invoke_params = 
        boost::shared_dynamic_cast<HttpInvokeParams>(invoke_params_);

    auto it = http_invoke_params->get_headers().find("SHS-DS-Expiration");
    if (it == http_invoke_params->get_headers().end())
    {
        return false;
    }

    return Timestamp::Now().MicroSecondsSinceEpoch() > 
        std::atoll(it->second.c_str());
}

} // namespace shs
