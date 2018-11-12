#include "event_watcher.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "log/logging.h"
#include "core/shs_epoll.h"
#include "core/shs_types.h"

#define BUFF_SZ 1024

namespace shs 
{

EventWatcher::EventWatcher()
{
}

EventWatcher::~EventWatcher() 
{
    Close();
}

bool EventWatcher::Init() 
{
    return DoInit();
}

void EventWatcher::Close() 
{
    DoClose();
}

PipedEventWatcher::PipedEventWatcher(event_base_t *event_base, 
    const Handler& handler)
{
    event_base_ = event_base;
    handler_ = handler;
    c_ = NULL;
}

void PipedEventWatcher::Notify() 
{
    if (shs_write_fd(pipe_[1], "C", 1) < 0) 
    {
        SLOG(ERROR) << "Send notify failed: " << strerror(errno);
    }
}

bool PipedEventWatcher::DoInit()
{
    conn_t *c = NULL;
    event_t *rev = NULL;
    
    if (pipe(pipe_) < 0)
    {
        SLOG(ERROR) << "pipe() failed";

        goto failed;
    }

    c = conn_get_from_mem(pipe_[0]);
    if (!c)
    {
        SLOG(ERROR) << "conn_get_from_mem() failed";

        goto failed;
    }

    conn_nonblocking(pipe_[0]);
    conn_nonblocking(pipe_[1]);

    c->ev_base = event_base_;
    c->conn_data = this;

    rev = c->read;
    rev->handler = PipedEventWatcher::HandlerFn;

    if (event_add(event_base_, rev, EVENT_READ_EVENT, 0) < 0)
    {
        SLOG(ERROR) << "event_add() failed";

        goto failed;
    }

    c_ = c;
    
    return true;

failed:
    Close();

    if (c)
    {
        conn_free_mem(c);        
    }

    return false;
}

void PipedEventWatcher::DoClose()
{
    if (pipe_[0] >= 0) 
    {
        close(pipe_[0]);
        pipe_[0] = -1;
    }

    if (pipe_[1] >=0)
    {
        close(pipe_[1]);
        pipe_[1] = -1;
    }

    if (c_)
    {
        conn_free_mem(c_);        
        c_ = NULL;
    }
}

void PipedEventWatcher::HandlerFn(event_t *ev) 
{
    conn_t *c = NULL;
    c = (conn_t *)ev->data;
    PipedEventWatcher *fn = (PipedEventWatcher *)c->conn_data;

    char buf[BUFF_SZ] = {0};
    ssize_t n = 0;
    bool failed = false;

    while (1)
    {
        n = shs_read_fd(fn->pipe_[0], buf, BUFF_SZ);
        if (n > 0 || (n < 0 && errno == SHS_EINTR))
        {
            continue;
        }

        if (0 == n) 
        {
            SLOG(ERROR) << "Notify fd closed";

            failed = true;
        } 
        else if (n < 0 && errno != SHS_EAGAIN)
        {
            SLOG(ERROR) << "Notify recv failed";

            failed = true;
        }

        break;
    }

    if (!failed)
    {
        fn->handler_();
    }
}

TimedEventWatcher::TimedEventWatcher(event_base_t *event_base, 
    event_timer_t *ev_timer, int timeout, const Handler& handler)
{
    event_base_ = event_base;
    ev_timer_ = ev_timer;
    timeout_ = timeout;
    handler_ = handler;

    memset(&ev_, 0x00, sizeof(event_t));
}

bool TimedEventWatcher::DoInit()
{
    ev_.data = this;
    ev_.handler = TimedEventWatcher::HandlerFn;
    event_timer_add(ev_timer_, &ev_, timeout_);

    return true;
}

void TimedEventWatcher::DoClose()
{
    if (ev_.timer_set)
    {
        event_timer_del(ev_timer_, &ev_);
    }

    memset(&ev_, 0x00, sizeof(event_t));
}

void TimedEventWatcher::HandlerFn(event_t *ev)
{
    TimedEventWatcher *fn = (TimedEventWatcher *)ev->data;
    fn->handler_();
}

} // namespace shs

