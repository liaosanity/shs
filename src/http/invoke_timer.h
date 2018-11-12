#pragma once

#include <stdint.h>
#include <boost/scoped_ptr.hpp>

#include "types.h"
#include "comm/timestamp.h"
#include "core/shs_epoll.h"
#include "core/shs_event_timer.h"

namespace shs 
{

class TimedEventWatcher;

class InvokeTimer 
{
public:
    typedef std::tr1::function<void()> TimeoutHandler;
    InvokeTimer(event_base_t *event_base,
        event_timer_t *event_timer, int timeout,
        const InvokeCompleteHandler& complete_handler,
        const TimeoutHandler& timeout_handler);
    ~InvokeTimer();

    void Cancel();
    void Complete(const InvokeResult& result) const;

    double ElapsedTime() const 
    { 
        return static_cast<double>(TimeDifference(Timestamp::Now(), 
            enqueue_time_)) / 1000000.0f; 
    }

    double ElapsedTimeInQueue() const
    {
        if (dequeue_time_.Valid())
        {
            return static_cast<double>(TimeDifference(dequeue_time_, 
                enqueue_time_)) / 1000000.0f; 
        }
        else
        {
            return ElapsedTime();
        }
    }

    void set_module(const std::string& name)
    {
        module_ = name;
    }

    const std::string& module() const { return module_; }

    void set_dequeue_time()
    {
        dequeue_time_ = Timestamp::Now();
    }

private:
    Timestamp enqueue_time_;
    Timestamp dequeue_time_;
    InvokeCompleteHandler complete_handler_;
    boost::scoped_ptr<TimedEventWatcher> timeout_watcher_;
    std::string module_;
};

} // namespace shs
