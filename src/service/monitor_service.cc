#include "monitor_service.h"

#include <string.h>

#include "log/logging.h"
#include "core/shs_event.h"
#include "core/shs_epoll.h"
#include "core/shs_socket.h"
#include "core/shs_memory.h"
#include "http/http_handler.h"

#include "config.h"
#include "stats.h"

namespace shs 
{

DEFINE_string(monitor_ip, "", "monitor page listen ip");
DEFINE_int32(monitor_port, 0, "monitor page listen port");

using namespace std;
using namespace boost;

MonitorService::MonitorService()
    : Service()
    , sockfd_(-1)
    , http_(NULL)
{
}

MonitorService::~MonitorService() 
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

bool MonitorService::Listen(int sockfd)
{
    if (sockfd < 0)
    {
        return false;
    }

    sockfd_ = sockfd;

    return true;
}

bool MonitorService::Listen(const char* host, int port)
{
    char tmp[11];
    sprintf(tmp, "%d", port);
    int fd = tcp_listen(host, tmp);
    if (fd < 0)
    {
        SLOG(ERROR) << "MonitorService::Listen() failed, port=" << port;

        return false;
    }

    sockfd_ = fd;

    return true;
}

bool MonitorService::ListenExpectAddress()
{
    if (FLAGS_monitor_ip.empty())
    {
        return Listen(NULL, FLAGS_monitor_port);
    }
    else
    {
        return Listen(FLAGS_monitor_ip.c_str(), FLAGS_monitor_port);
    }
}

int MonitorService::ListenFD() const
{
    return sockfd_;
}

bool MonitorService::RegisterEvent(event_base_t* base, event_timer_t *timer, 
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
    http_->stat_cb = HttpStatsHandler;

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

void MonitorService::Stop() 
{
    if (http_)
    {
        event_done(event_base_);
    }
}

void MonitorService::Pause() 
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

void MonitorService::Continue() 
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
