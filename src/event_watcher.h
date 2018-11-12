#pragma once

#include <tr1/functional>
#include <boost/noncopyable.hpp>

#include "core/shs_conn.h"
#include "core/shs_event.h"
#include "core/shs_event_timer.h"

namespace shs 
{

class EventWatcher : private boost::noncopyable
{
public:
    typedef std::tr1::function<void()> Handler;

    EventWatcher();
    virtual ~EventWatcher();

    bool Init();
    void Close();

protected:
    virtual bool DoInit() = 0;
    virtual void DoClose() {}

    Handler handler_;
    event_base_t *event_base_;
};

class PipedEventWatcher : public EventWatcher
{
public:
    PipedEventWatcher(event_base_t *event_base, const Handler& handler);

    void Notify();

private:
    virtual bool DoInit();
    virtual void DoClose();
    static void HandlerFn(event_t *ev);

    conn_t *c_;
    int pipe_[2];
};

class TimedEventWatcher : public EventWatcher
{
public:
    TimedEventWatcher(event_base_t *event_base, event_timer_t *ev_timer, 
        int timeout, const Handler& handler);

private:
    virtual bool DoInit();
    virtual void DoClose();
    static void HandlerFn(event_t *ev);

    event_t ev_;
    event_timer_t *ev_timer_;
    int timeout_;
};

} // namespace shs

