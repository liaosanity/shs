#pragma once

#include "service.h"
#include "http/http.h"

namespace shs 
{

class HTTPService : public Service 
{
public:
    HTTPService();
    ~HTTPService();

    bool Listen(const char* host, int port);
    bool Listen(int sockfd);
    int ListenFD() const;
    bool ListenExpectAddress();

    bool RegisterEvent(event_base_t* base, event_timer_t *timer, 
        conn_pool_t* pool);
    void Stop();
    void Continue();
    void Pause();

    std::string GetType() const { return "Http"; }

private:
    int sockfd_;
    http_srv_t* http_;
};

} // namespace shs
