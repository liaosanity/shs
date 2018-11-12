#pragma once

#include <string>
#include <boost/shared_ptr.hpp>

#include "core/shs_conn_pool.h"
#include "core/shs_epoll.h"
#include "core/shs_event_timer.h"

namespace shs 
{

struct ServiceStatus 
{
    enum status
    {
        NONE = 0,
        STOPPED,
        PAUSED,
        RUNNING
    };
};

class Framework;

class Service 
{
public:
    Service();
    virtual ~Service() {}

    virtual void Init(Framework* framework);
    virtual bool Listen(const char* host, int port) = 0;
    virtual bool Listen(int sockfd) = 0;
    virtual bool ListenExpectAddress() = 0;
    virtual int ListenFD() const = 0;

    virtual bool RegisterEvent(event_base_t* base, event_timer_t *timer, 
        conn_pool_t* pool) = 0;
    virtual void Stop() = 0;
    virtual void Continue() = 0;
    virtual void Pause() = 0;
    virtual std::string GetType() const = 0;

protected:
    Framework* framework_;
    event_base_t *event_base_;
    conn_pool_t* conn_pool_;
    event_timer_t *event_timer_;
    int status_;
};

typedef boost::shared_ptr<Service> ServicePtr;

} // namespace shs
