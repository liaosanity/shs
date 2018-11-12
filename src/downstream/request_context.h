#pragma once

#include <set>
#include <tr1/functional>

#include "downstream/server.h"
#include "comm/timestamp.h"
#include "http/http.h"

namespace shs 
{ 
namespace downstream 
{

class RequestContext
{
public:
    RequestContext()
        : retry_num(0)
        , hash(0)
        , err_code(OK)
        , history_hosts()
        , server(NULL)
        , create_timestamp(Timestamp::Now())
        , req(NULL)
        , host(NULL)
        , hc(NULL)
    {
    }

    virtual ~RequestContext()
    {
        if (hc)
        {
            http_conn_free(hc);
            hc = NULL;
        }
    }

    bool TryAgain()
    {
        if (retry_num >= max_retry_num())
        {
            return false;
        }

        retry_num++;
        req->Execute(this);

        return true;
    }

    uint32_t max_retry_num() const
    {
        return server->max_retry_num();
    }

public:
    uint32_t retry_num;
    size_t hash;
    ErrCode err_code;
    std::set<int> history_hosts;
    Server *server;
    Timestamp create_timestamp;
    Request* req;
    Host* host;
    http_conn_t* hc;
    ResponseHandler response_handler;
};

} // namespace downstream
} // namespace shs
