#include "request.h"

#include <string>
#include <tr1/functional>
#include <boost/format.hpp>
#include <gflags/gflags.h>

#include "log/logging.h"
#include "http/http.h"
#include "comm/timestamp.h"
#include "downstream/host.h"
#include "downstream/util.h"
#include "downstream/request_context.h"
#include "downstream/response.h"

namespace shs 
{ 
namespace downstream 
{ 

using namespace std;

namespace 
{

ErrCode ConvertToErrCode(HTTP_CODE qec, 
    http_req_t* rsp, RequestContext* ctx)
{
    ErrCode ec = OK;

    switch (qec) 
    {
    case HTTP_CODE_OK:
        ec = OK;
        break;
    default:
        ec = static_cast<ErrCode>(qec + 1000);
        break;
    }

    if (rsp && qec == HTTP_CODE_OK)
    {
        if (rsp->response_code == 200)
        {
            ec = OK;
        }
        else
        {
            ec = E_BAD_RESPONSE;
        }
    } 
    else 
    {
        if (ctx->err_code != OK)
        {
            ec = ctx->err_code;
        }
    }

    if (ec != OK)
    {
        ctx->err_code = ec;
    }

    return ec;
}

} // namespace

RequestUri::RequestUri(const string& base)
    : base_uri_(base)
{
}

RequestUri::~RequestUri()
{
}

void RequestUri::AddQuery(const map<string, string>& query)
{
    query_.insert(query.begin(), query.end());
}

void RequestUri::AddQuery(const string& name, const string& value)
{
    query_[name] = value;
}

string RequestUri::uri()
{
    string uri = base_uri_;
    map<string, string>::iterator iter;
    for (iter = query_.begin(); iter != query_.end(); ++iter)
    {
        std::string encoded_value = http_encode_uri(iter->second.c_str());

        if (iter == query_.begin())
        {
            uri += boost::str(boost::format("?%1%=%2%") 
                % iter->first % encoded_value);
        }
        else
        {
            uri += boost::str(boost::format("&%1%=%2%") 
                % iter->first % encoded_value);
        }
    }

    return uri;
}

Request::Request(const std::string& uri, const std::string& body, 
    const std::string& hash, HttpType type)
    : uri_(uri)
    , body_(body)
    , type_(type)
{
    if (hash.length() != 0)
    {
        hash_ = CalcHash(hash.c_str(), hash.length());
    }
    else if (uri.length() != 0)
    {
        hash_ = CalcHash(uri.c_str(), uri.length());
    }
}

Request::~Request()
{
}

static void HandlerResponse(HTTP_CODE qec, 
    http_req_t* rsp, void* data) 
{
    RequestContext* ctx = (RequestContext *)data;
    ErrCode ec = ConvertToErrCode(qec, rsp, ctx);
    std::string ip = ctx->host->ip();
    int port = ctx->host->port();
    boost::shared_ptr<Response> response;

    if (ec == OK)
    {
        response.reset(new Response(rsp, ip, ctx->retry_num));
    }
    else
    {
        SLOG(ERROR) << "Get Response failed"
            << "\thost(" << ip << ":" << port << ")"
            << "\turi=" << ctx->req->uri()
            << "\tretry_num=" << ctx->retry_num
            << "\tmax_retry_num=" << ctx->max_retry_num()
            << "\thash=" << ctx->hash
            << "\tec=" << ec
            << "\tmsg=" << ErrCodeToString(ec);

        if (ctx->TryAgain())
        {
            return;
        }

        response.reset(new Response(NULL, ip, ctx->retry_num));
    }

    if (rsp)
    {
        response->set_code(rsp->response_code);
    }

    try
    {
        ctx->response_handler(ec, response);
    }
    catch (...)
    {
        SLOG(ERROR) << "Get Exception" 
            << "\thost(" << ip << ":" << port << ")"
            << "\turi=" << ctx->req->uri();
    }

    delete ctx;
}

void Request::AddHeader(const std::string& key, const std::string& value)
{
    headers_[key] = value;
}

void Request::Execute(RequestContext* ctx)
{
    pool_t *mempool = NULL;
    pool_t *mempool2 = NULL;
    http_conn_t *hc = NULL;
    http_req_t *req = NULL;
    int conn_timeout = -1;
    int recv_timeout = -1;
    int retry_cnt = -1;

    ctx->host = ctx->server->Create(ctx->hash, ctx->retry_num,
        ctx->history_hosts, &ctx->err_code);
    if (!ctx->host)
    {
        goto failed;
    }

    if (ctx->hc)
    {
        http_conn_free(ctx->hc);
        ctx->hc = NULL;
    }

    mempool = pool_create(CONN_DEFAULT_POOL_SIZE, CONN_DEFAULT_POOL_SIZE);
    if (!mempool)
    {
        goto failed;
    }
 
    hc = (http_conn_t *)pool_calloc(mempool, sizeof(http_conn_t));
    if (!hc)
    {
        pool_destroy(mempool);

        goto failed;
    }

    hc->mempool = mempool;
    hc->base = ctx->server->event_base();
    hc->timer = ctx->server->event_timer();
    hc->connpool = ctx->server->conn_pool();
    strcpy(hc->host, ctx->host->ip().c_str());
    hc->port = ctx->host->port(); 

    hc->c = conn_pool_get_connection(hc->connpool);
    if (!hc->c)
    {
        pool_destroy(mempool);

        goto failed;
    }

    conn_timeout = ctx->server->option().timeout_con;
    if (conn_timeout > 0)
    {
        http_conn_set_connect_timeout_ms(hc, conn_timeout);
    }

    recv_timeout = ctx->server->option().timeout_rcv;
    if (recv_timeout > 0)
    {
        http_conn_set_recv_timeout_ms(hc, recv_timeout);
    }

    retry_cnt = ctx->server->option().max_retry_con_num;
    if (retry_cnt > 0)
    {
        http_conn_set_retries(hc, retry_cnt);
    }

    ctx->hc = hc;

    mempool2 = pool_create(CONN_DEFAULT_POOL_SIZE, CONN_DEFAULT_POOL_SIZE);
    if (!mempool2)
    {
        goto failed;
    }

    req = (http_req_t *)pool_calloc(mempool2, sizeof(http_req_t));
    if (!req)
    {
        pool_destroy(mempool2);

        goto failed;
    }

    req->mempool = mempool2;
    req->data = ctx;
    req->cb = HandlerResponse;
    req->input_body = string_null;
    req->output_body = string_null;
    req->uri = string_null;
    req->response_code_line = string_null;
    http_init_headers(req);
    req->hc = hc;
    hc->req = req;

    for (auto& header : headers_)
    {
        http_add_output_header(req, header.first, header.second);
    }

    if (!http_exist_header(req->output_headers, "host"))
    {
        http_add_output_header(req, "host", ctx->host->ip());
    }

    if (!http_exist_header(req->output_headers, "SHS-DS-Retry"))
    {
        http_add_output_header(req, "SHS-DS-Retry", 
            std::to_string(ctx->retry_num));
    }

    if (!http_exist_header(req->output_headers, "SHS-DS-Expiration"))
    {
        Timestamp end = AddTime(ctx->create_timestamp, 
            ctx->server->max_timeout() * 1000);
        http_add_output_header(req, "SHS-DS-Expiration",
            std::to_string(end.MicroSecondsSinceEpoch()));
    }

    if (http_make_request(req, type_, uri_, body_) != SHS_OK)
    {
        ctx->err_code = E_HTTP_CONNECT_FAIL;
        HandlerResponse(HTTP_CODE_CALLER_ERROR, NULL, ctx);

        return;
    }

    ctx->err_code = OK;

    return;

failed:
    boost::shared_ptr<Response> response(new Response(NULL));
    ctx->response_handler(E_BAD_REQUEST, response);

    delete ctx;
}

void Request::Execute(Server *server, 
    const ResponseHandler& response_handler)
{
    if (NULL == server)
    {
        boost::shared_ptr<Response> response(new Response(NULL));
        response_handler(E_BAD_REQUEST, response);

        return;
    }

    RequestContext* ctx = new RequestContext();
    if (NULL == ctx)
    {
        boost::shared_ptr<Response> response(new Response(NULL));
        response_handler(E_BAD_REQUEST, response);

        return;
    }

    ctx->hash = hash_;
    ctx->req = this;
    ctx->retry_num = 0;
    ctx->server = server;
    ctx->response_handler = response_handler;

    Execute(ctx);
}

GetRequest::GetRequest(const std::string& uri, const std::string& hash)
    : Request(uri, "", hash, SHS_HTTP_REQ_TYPE_GET)
{
}

PostRequest::PostRequest(const std::string& uri, const std::string& body, 
    const std::string& hash)
    : Request(uri, body, hash, SHS_HTTP_REQ_TYPE_POST) 
{
}

} // namespace downstream
} // namespace shs
