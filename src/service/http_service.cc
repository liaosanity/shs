#include "http_service.h"

#include <string.h>
#include <boost/filesystem.hpp>

#include "log/logging.h"
#include "core/shs_socket.h"
#include "core/shs_memory.h"
#include "http/http_handler.h"

#include "config.h"
#include "stats.h"

namespace shs 
{

DEFINE_int32(http_port, 0, "Listen port");

using namespace std;
using namespace boost;

HTTPService::HTTPService()
    : Service()
    , sockfd_(-1)
    , http_(NULL)
{
}

HTTPService::~HTTPService() 
{
    if (event_base_)
    {
        event_done(event_base_);
    }

    if (sockfd_ >= 0)
    {
        close(sockfd_);
        sockfd_ = -1;
    }

    if (http_ && http_->c)
    {
        conn_free_mem(http_->c);
    }

    ProcessStats::SetServerConns(0);

    if (http_)
    {
        memory_free(http_, sizeof(http_srv_t));
        http_ = NULL;
    }
}

bool HTTPService::Listen(int sockfd)
{
    if (sockfd < 0)
    {
        return false;
    }

    sockfd_ = sockfd;

    return true;
}

bool HTTPService::Listen(const char* host, int port)
{
    char tmp[11];
    sprintf(tmp, "%d", port);
    int fd = tcp_listen(host, tmp);
    if (fd < 0)
    {
        SLOG(ERROR) << "HTTPService::Listen() failed, port=" << port;

        return false;
    }

    sockfd_ = fd;

    return true;
}

int HTTPService::ListenFD() const
{
    return sockfd_;
}

bool HTTPService::ListenExpectAddress()
{
    return Listen(NULL, FLAGS_http_port);
}

bool HTTPService::RegisterEvent(event_base_t* base, event_timer_t *timer, 
    conn_pool_t* pool) 
{
    event_base_ = base;
    event_timer_ = timer;
    conn_pool_ = pool;

    http_ = (http_srv_t *)memory_calloc(sizeof(http_srv_t));
    if (!http_)
    {
        SLOG(ERROR) << "new HttpServer() failed";

        return false;
    }

    http_->data = framework_;
    http_->base = base;
    http_->timer = timer;
    http_->conn_pool = pool;
    http_->stat_cb = HttpStatusHandler;
    http_->req_cb = HttpReqHandler;

    conn_t *c = conn_get_from_mem(sockfd_);
    if (!c) 
    {
        SLOG(ERROR) << "conn_get_from_mem() failed";

        if (http_)
        {
            memory_free(http_, sizeof(http_srv_t));
            http_ = NULL;
        }

        return false;
    }
    
    c->conn_data = http_;
    http_->c = c;

    event_t *rev = c->read;
    rev->accepted = SHS_TRUE;
    rev->handler = http_accept_handler;

    if (event_add(event_base_, rev, EVENT_READ_EVENT, 0) < 0)
    {
        if (http_)
        {
            memory_free(http_, sizeof(http_srv_t));
            http_ = NULL;
        }

        conn_free_mem(c);

        return false;
    }

    return true;
}

void HTTPService::Stop() 
{
    if (http_)
    {
        event_done(event_base_);
    }
}

void HTTPService::Pause() 
{
    if (status_ == ServiceStatus::RUNNING)
    {
        status_ = ServiceStatus::PAUSED;

        conn_t *c = http_->c;
        if (event_delete(event_base_, c->read, EVENT_READ_EVENT, 0) < 0)
        {
            SLOG(ERROR) << "event_delete() failed";
        }
    }
}

void HTTPService::Continue() 
{
    if (status_ == ServiceStatus::PAUSED)
    {
        status_ = ServiceStatus::RUNNING;

        conn_t *c = http_->c;
        if (event_add(event_base_, c->read, EVENT_READ_EVENT, 0) < 0)
        {
            SLOG(ERROR) << "event_add() failed";
        }
    }
}

} // namespace shs
