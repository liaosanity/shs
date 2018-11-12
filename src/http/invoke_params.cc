#include "invoke_params.h"
#include "invoke_timer.h"

namespace shs 
{

InvokeParams::InvokeParams()
    : request_time_(0)
    , enqueue_time_(0)
    , dequeue_time_(0)
    , client_port_(0)
    , task_queue_size_(0)
    , timer_queue_size_(0)
{
}

void InvokeParams::set_request_time(double t)
{
    request_time_ = t;
}

void InvokeParams::set_enqueue_time(double t)
{
    enqueue_time_ = t;
}

void InvokeParams::set_dequeue_time(double t)
{
    dequeue_time_ = t;
}

void InvokeParams::set_client_ip(const std::string& ip)
{
    client_ip_ = ip;
}

void InvokeParams::set_client_port(uint16_t port)
{
    client_port_ = port;
}

void InvokeParams::set_task_queue_size(size_t size)
{
    task_queue_size_ = size;
}

void InvokeParams::set_timer_queue_size(size_t size)
{
    timer_queue_size_ = size;
}

} // namespace shs
