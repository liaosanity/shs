#pragma once

#include <boost/scoped_ptr.hpp>

#include "service.h"
#include "http/http.h"

namespace shs 
{

class MonitorService : public Service
{
public:
    MonitorService();
    ~MonitorService();

    bool Listen(const char* host, int port);
    bool Listen(int sockfd);
    bool ListenExpectAddress();

    int ListenFD() const;

    bool RegisterEvent(event_base_t* base, event_timer_t *timer, 
        conn_pool_t* pool);

    void Stop();
    void Continue();
    void Pause();

    std::string GetType() const { return "Monitor"; }

private:
    int sockfd_;
    http_srv_t* http_;
};

} // namespace shs
