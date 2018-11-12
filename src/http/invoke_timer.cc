#include "invoke_timer.h"
#include "event_watcher.h"

namespace shs 
{

InvokeTimer::InvokeTimer(event_base_t *event_base, 
    event_timer_t *event_timer, int timeout,
    const InvokeCompleteHandler& complete_handler,
    const TimeoutHandler& timeout_handler)
    : complete_handler_(complete_handler)
{
    timeout_watcher_.reset(new TimedEventWatcher(event_base, event_timer, 
        timeout, timeout_handler));
    timeout_watcher_->Init();

    enqueue_time_ = Timestamp::Now();
}

InvokeTimer::~InvokeTimer()
{
}

void InvokeTimer::Cancel() 
{
    timeout_watcher_->Close();
}

void InvokeTimer::Complete(const InvokeResult& result) const 
{
    complete_handler_(result);
}

} // namespace shs
