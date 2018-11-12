#include "http.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <sstream>
#include <gflags/gflags.h>

#include "log/logging.h"
#include "core/shs_buffer.h"
#include "core/shs_event.h"
#include "core/shs_memory_pool.h"
#include "core/shs_event_timer.h"
#include "core/shs_memory.h"
#include "core/shs_epoll.h"

#include "stats.h"

namespace shs 
{

DECLARE_string(default_module);
DEFINE_bool(disable_http_keepalive, false, "disable http 1.1 keepalive");

static int http_get_request_with_connection(http_conn_t *);
static void event_process_handler(event_t *);
static void conn_read_handler(http_conn_t *);
static void conn_write_handler(http_conn_t *);
static int conn_recv(http_req_t *);
static int conn_send(http_req_t *);
static void conn_wait_connect_handler(http_conn_t *);
static char *buffer_readline(http_req_t *);
static char *strsep(char **, const char *);
static void http_read_header(http_conn_t *, http_req_t *);
static void http_read_body(http_conn_t *, http_req_t *);
static void http_conn_done(http_conn_t *, http_req_t *);
static void http_handle_request(HTTP_CODE, http_req_t *, void *);
static void http_send_done(http_conn_t *, http_req_t *);
static void http_retry_connect(HTTP_CODE, http_conn_t *);
static void http_send_request(http_conn_t *);
static void http_send_request_done(http_conn_t *, http_req_t *);
static void request_block_writing(http_conn_t *);
static void http_write_buffer(http_conn_t *);
static void http_conn_fail(http_conn_t *, enum HTTP_CODE);
static void http_response_code(http_req_t *, int, const std::string&);
static void http_send_page(http_req_t *, const std::string&);
static void http_request_free(http_req_t *);
static int http_conn_connect(http_conn_t *);
static bool http_find_header(const http_header_t headers[], 
    const std::string&, std::string *);

namespace 
{

const char* http_method(http_cmd_type type)
{
    const char *method;

    switch (type) 
    {
    case SHS_HTTP_REQ_TYPE_GET:
        method = "GET";
        break;

    case SHS_HTTP_REQ_TYPE_POST:
        method = "POST";
        break;

    case SHS_HTTP_REQ_TYPE_HEAD:
        method = "HEAD";
        break;

    default:
        method = NULL;
        break;
    }

    return method;
}

int64_t GetTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

int64_t GetTimestampDiffrenceInMicroSeconds(int64_t start)
{
    return (GetTimestamp() - start);
}

} // namespace

static const char* html_replace(char ch, char *buf)
{
    switch (ch) 
    {
    case '<':
        return "&lt;";
    case '>':
        return "&gt;";
    case '"':
        return "&quot;";
    case '\'':
        return "&#039;";
    case '&':
        return "&amp;";
    default:
        break;
    }

    buf[0] = ch;
    buf[1] = '\0';

    return buf;
}

static std::string http_htmlescape(const char *html)
{
    std::string result;
    size_t len = strlen(html);
    char scratch_space[2];
    for (size_t i = 0; i < len; ++i)
    {
        result.append(html_replace(html[i], scratch_space));
    }

    return result;
}

static void conn_write_handler(http_conn_t *hc)
{
    if (hc->c->write->timedout)
    {
        http_conn_fail(hc, HTTP_CODE_WRITE_TIMEOUT);

        return;
    }

    if (hc->c->write->timer_set)
    {
        event_timer_del(hc->c->ev_timer, hc->c->write);
    }

    if (hc->flags & HTTP_FLAGS_OUTGOING)
    {
        hc->conn_cb = http_send_request_done;
        http_write_buffer(hc);
    }
    else
    {
        hc->conn_cb = http_send_done;
        http_write_buffer(hc);
    }
}

static void http_write_buffer(http_conn_t* hc)
{
    http_req_t *req = hc->req;

    int n = conn_send(req);
    if (0 == n || SHS_ERROR == n)
    {
        http_conn_fail(hc, HTTP_CODE_WRITE_EOF);

        return;
    }
    else if (n > 0)
    {
        if (hc->conn_cb)
        {
            hc->conn_cb(hc, req);
        }

        return;
    }

    hc->write_event_handler = conn_write_handler;

    conn_t *c = hc->c;
    event_t *wev = c->write;
    event_handle_write(c->ev_base, wev, 0);
    event_timer_add(c->ev_timer, wev, hc->timeout_send);
}

static int conn_send(http_req_t *req)
{
    buffer_shrink(req->out);

    int n = 0;
    size_t size = buffer_size(req->out);
    size_t done = 0;
    conn_t *c = req->hc->c; 

    while (size)
    {
        n = c->send(c, req->out->pos, size);
        if (n <= 0)
        {
            return n;
        }

        req->out->pos += n;
        size -= n;
        done += n;
    }

    buffer_reset(req->out);

    return done;
}

static void conn_wait_connect_handler(http_conn_t *hc)
{
    conn_t *c = hc->c;
    if (c->write->timedout)
    {
        http_retry_connect(HTTP_CODE_CONNECT_TIMEOUT, hc);

        return;
    }

    if (c->write->timer_set)
    {
        event_timer_del(c->ev_timer, c->write);
    }

    int error;
    socklen_t errsz = sizeof(error);
    if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, 
        reinterpret_cast<void*>(&error), &errsz) == -1) 
    {
        http_retry_connect(HTTP_CODE_INVALID_SOCKET, hc);

        return;
    }

    if (error) 
    {
        http_retry_connect(HTTP_CODE_CONNECT_FAIL, hc);

        return;
    }

    hc->retry_cnt = 0;
    hc->status = HTTP_STATUS_IDLE;

    http_send_request(hc);
}

static void http_make_request_header(http_req_t *req)
{
    const char* method = http_method(req->type);
    char buf[HEADER_SZ] = {0};
    sprintf(buf, "%s %s HTTP/%d.%d\r\n", method, req->uri.data, 
        req->major, req->minor);
    int blen = strlen(buf);
    memcpy(req->out->last, buf, blen);
    req->out->last += blen;

    if (SHS_HTTP_REQ_TYPE_POST == req->type 
        && !http_exist_header(req->output_headers, "Content-Length")) 
    {
        http_add_output_header(req, "Content-Length", 
            std::to_string(req->output_body.len));
    }
}

static bool http_is_connection_close(int flags, const http_header_t headers[])
{
    std::string connection;
    return (http_find_header(headers, "Connection", &connection) 
        && 0 == strncasecmp(connection.c_str(), "close", 5));
}

static bool http_is_connection_keepalive(const http_header_t headers[])
{
    std::string connection;
    return (http_find_header(headers, "Connection", &connection)
        && 0 == strncasecmp(connection.c_str(), "keep-alive", 10));
}

static void http_add_date_header(http_req_t *req)
{
    if (!http_exist_header(req->output_headers, "Date")) 
    {
        char date[50];
        struct tm cur;
        struct tm *cur_p;
        time_t t = time(NULL);
        gmtime_r(&t, &cur);
        cur_p = &cur;
        if (strftime(date, sizeof(date),
            "%a, %d %b %Y %H:%M:%S GMT", cur_p) != 0) 
        {
            http_add_output_header(req, "Date", date);
        }
    }
}

static void http_add_content_length_header(http_req_t *req, 
    long content_length)
{
    if (!http_exist_header(req->output_headers, "Transfer-Encoding") 
        && !http_exist_header(req->output_headers, "Content-Length")) 
    {
        http_add_output_header(req, "Content-Length", 
            std::to_string(content_length));
    }
}

static void http_make_response_header(http_req_t *req)
{
    bool is_keepalive = http_is_connection_keepalive(req->input_headers);
    if (FLAGS_disable_http_keepalive)
    {
        is_keepalive = false;
    }

    auto minor = req->minor;
    if (FLAGS_disable_http_keepalive)
    {
        minor = 0;
    }

    char buf[HEADER_SZ] = {0};
    sprintf(buf, "HTTP/%d.%d %d %s\r\n", req->major, minor, req->response_code, 
        req->response_code_line.data);
    int blen = strlen(buf);
    memcpy(req->out->last, buf, blen);
    req->out->last += blen;

    if (1 == req->major) 
    {
        if (1 == minor)
        {
            http_add_date_header(req);
        }

        if (0 == minor && is_keepalive) 
        {
            http_add_output_header(req, "Connection", "keep-alive");
        }

        http_add_content_length_header(req, req->output_body.len);
    }

    if (0 != req->output_body.len) 
    {
        if (!http_exist_header(req->output_headers, "Content-Type")) 
        {
            http_add_output_header(req, "Content-Type", 
                "text/html; charset=ISO-8859-1");
        }
    }

    if (FLAGS_disable_http_keepalive 
        || http_is_connection_close(req->flags, req->input_headers)) 
    {
        http_add_output_header(req, "Connection", "close");
    }
}

static void http_make_header(http_req_t *req)
{
    if (SHS_HTTP_KIND_REQUEST == req->kind) 
    {
        http_make_request_header(req);
    } 
    else 
    {
        http_make_response_header(req);
    }

    char buf[HEADER_SZ] = {0};
    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 != req->output_headers[i].key.len 
            && 0 != req->output_headers[i].value.len)
        {
            sprintf(buf, "%s: %s\r\n", req->output_headers[i].key.data, 
                req->output_headers[i].value.data);
            int blen = strlen(buf);
            memcpy(req->out->last, buf, blen);
            req->out->last += blen;
            memset(buf, 0x00, HEADER_SZ);
        }
    }
    memcpy(req->out->last, "\r\n", 2);
    req->out->last += 2;

    if (req->output_body.len != 0) 
    {
        size_t head_len = buffer_size(req->out);
        size_t body_len = req->output_body.len;
        size_t out_len = head_len + body_len;
        buffer_t *out = buffer_create(req->mempool, out_len);
        if (!out)
        {
            SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                << " buffer_create() failed.";
        }
        
        memcpy(out->pos, req->out->pos, head_len);
        out->last += head_len;
        buffer_reset(req->out);
        memcpy(out->last, req->output_body.data, body_len);
        out->last += body_len;

        req->out = out;
    }
}

static int http_incoming_fail(http_req_t *req, HTTP_CODE ec)
{
    switch (ec) 
    { 
    case HTTP_CODE_READ_TIMEOUT:
    case HTTP_CODE_READ_EOF:
    case HTTP_CODE_WRITE_TIMEOUT:
    case HTTP_CODE_WRITE_EOF:
        if (!req->userdone) 
        {
            req->hc->req = NULL;
            req->hc = NULL;
        }
        return -1;
    case HTTP_CODE_INVALID_HEADER:
    case HTTP_CODE_INVALID_BODY:
        req->uri = string_null;
        if (req->cb) 
        {
            req->cb(ec, req, req->data);
        }
        break;
    default:
        break;
    }
    
    return 0;
}

static void http_conn_fail(http_conn_t *hc, HTTP_CODE ec)
{
    http_req_t *req = hc->req;

    if (hc->flags & HTTP_FLAGS_INCOMING) 
    {
        if (http_incoming_fail(req, ec) < 0)
        {
            http_conn_free(hc);
        }

        return;
    }

    hc->req = NULL;

    if (req->cb)
    {
        req->cb(ec, req, req->data);
    }

    http_request_free(req);
}

static void http_conn_reset(http_conn_t *hc)
{
    hc->status = HTTP_STATUS_DISCONNECTED;
}

static void conn_close_handler(http_conn_t *hc)
{
    http_conn_reset(hc);
}

static void http_conn_start_detectclose(http_conn_t *hc)
{
    hc->flags |= HTTP_FLAGS_CLOSEDETECT;
    hc->read_event_handler = conn_close_handler;
}

static void http_conn_stop_detectclose(http_conn_t *hc)
{
    hc->flags &= ~HTTP_FLAGS_CLOSEDETECT;
}

static void http_conn_done(http_conn_t *hc, http_req_t *req)
{
    int con_outgoing = hc->flags & HTTP_FLAGS_OUTGOING;

    if (con_outgoing) 
    {
        hc->req = NULL;
        req->hc = NULL;

        hc->status = HTTP_STATUS_IDLE;

        bool need_close = 
            http_is_connection_close(req->flags, req->input_headers) 
            || http_is_connection_close(req->flags, req->output_headers);
        if (need_close)
        {
            http_conn_reset(hc);
        }
        else
        {
            http_conn_start_detectclose(hc);
        }
    } 
    else if (hc->status != HTTP_STATUS_DISCONNECTED) 
    {
        hc->status = HTTP_STATUS_WRITING; 
    }

    if (req->cb) 
    {    
        req->cb(HTTP_CODE_OK, req, req->data);
    }

    if (con_outgoing) 
    {
        http_request_free(req);
    }
}

static void http_read_body(http_conn_t *hc, http_req_t *req)
{
    buffer_t *buf = req->in;
    size_t blen = buffer_size(buf);
    req->read_done += blen;

    if (req->read_done >= req->ntoread) 
    {
        if (0 == req->input_body.len)
        {
            req->input_body.data = buf->pos;
            req->input_body.len = req->ntoread;
        }
        else
        {
            size_t len = req->input_body.len + blen;
            uchar_t *data = (uchar_t *)pool_calloc(req->mempool, len);
            if (!data)
            {
                SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                    << " pool_calloc() failed."; 
            }
            memory_memcpy(data, req->input_body.data, req->input_body.len);
            memory_memcpy(data + req->input_body.len, buf->pos, blen);
            req->input_body.data = data;
            req->input_body.len = len;
        }

        buf->pos += blen;
        req->ntoread = 0;
        req->read_done = 0;

        http_conn_done(hc, req);

        return;
    }
    else if (blen > 0)
    {
        if (0 == req->input_body.len)
        {
            req->input_body.data = (uchar_t *)pool_calloc(
                req->mempool, blen);
            if (!req->input_body.data)
            {
                SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                    << " pool_calloc() failed."; 
            }
            memory_memcpy(req->input_body.data, buf->pos, blen);
            req->input_body.len = blen;
        }
        else
        {
            size_t len = req->input_body.len + blen;
            uchar_t *data = (uchar_t *)pool_calloc(req->mempool, len);
            if (!data)
            {
                SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                    << " pool_calloc() failed.";
            }
            memory_memcpy(data, req->input_body.data, req->input_body.len);
            memory_memcpy(data + req->input_body.len, buf->pos, blen);
            req->input_body.data = data;
            req->input_body.len = len;
        }

        buf->pos += blen;
    }

    conn_t *c = hc->c;
    event_t *rev = c->read;

    if (rev->active && rev->ready)
    {
        conn_read_handler(hc);

        return;
    }

    event_handle_read(c->ev_base, rev, 0);
}

static void http_send_request_done(http_conn_t *hc, http_req_t *req)
{
    assert(hc->status == HTTP_STATUS_WRITING); 
    req->kind = SHS_HTTP_KIND_RESPONSE;

    request_block_writing(hc);

    hc->write_event_handler = NULL;
    hc->read_event_handler = conn_read_handler;
    hc->status = HTTP_STATUS_READING_FIRSTLINE;

    conn_t *c = hc->c;
    event_t *rev = c->read;
    event_handle_read(c->ev_base, rev, 0);
    event_timer_add(c->ev_timer, rev, hc->timeout_recv);
}

static void request_block_writing(http_conn_t *hc)
{
    conn_t *c = hc->c;
    event_t *wev = c->write;
    event_delete(c->ev_base, wev, EVENT_WRITE_EVENT, EVENT_CLEAR_EVENT);
}

void http_conn_free(http_conn_t *hc)
{
    if (hc->req)
    {
        http_request_free(hc->req);
        hc->req = NULL;
    }

    http_srv_t *http = hc->http_srv;
    if (http)
    {
        ProcessStats::SetServerConns(--http->connections);
    }

    if (hc->c)
    {
        conn_release(hc->c);
        conn_pool_free_connection(hc->connpool, hc->c);
    }

    hc->http_srv = NULL;
    hc->connpool = NULL;
    hc->c = NULL;
    hc->base = NULL;
    hc->timer = NULL;
    hc->read_event_handler = NULL;
    hc->write_event_handler = NULL;
    hc->conn_cb = NULL;

    if (hc->mempool)
    {
        pool_destroy(hc->mempool);
    }
}

static void http_send_request(http_conn_t* hc)
{
    http_req_t *req = hc->req;
    if (!req)
    {
        return;
    }

    http_conn_stop_detectclose(hc);

    assert(hc->status == HTTP_STATUS_IDLE);
    hc->status = HTTP_STATUS_WRITING;

    http_make_header(req);

    hc->conn_cb = http_send_request_done;
    http_write_buffer(hc); 
}

static void http_retry_connect(HTTP_CODE ec, http_conn_t* hc)
{
    if (hc->retry_cnt < hc->retry_max) 
    {
        hc->retry_cnt++;

        int ret = http_conn_connect(hc);
        if (SHS_ERROR == ret) 
        {
            http_retry_connect(HTTP_CODE_CONNECT_FAIL, hc);

            return;
        } 
        else if (SHS_OK == ret)
        {
            http_send_request(hc);
            
            return;
        }

        conn_t *c = hc->c;
        c->conn_data = hc;
        event_t *rev = c->read;
        event_t *wev = c->write; 
        rev->handler = event_process_handler;
        wev->handler = event_process_handler;

        hc->write_event_handler = conn_wait_connect_handler;

        event_timer_add(c->ev_timer, wev, hc->timeout_connect);

        return;
    }

    http_conn_reset(hc);

    http_req_t *req = hc->req;
    hc->req = NULL;

    if (req)
    {
        if (req->cb) 
        {
            req->cb(ec, req, req->data);
        }

        http_request_free(req);
    }
}

static int http_valid_response_code(int code)
{
    return (0 == code ? 0 : 1);
}

static int http_parse_response_line(http_req_t *req, char *line)
{
    const char *readable = "";

    char* protocol = strsep(&line, " ");
    if (NULL == line)
    {
        return -1;
    }

    char* number = strsep(&line, " ");
    if (NULL != line)
    {
        readable = line;
    }

    if (0 == strcmp(protocol, "HTTP/1.0")) 
    {
        req->major = 1;
        req->minor = 0;
    } 
    else if (0 == strcmp(protocol, "HTTP/1.1")) 
    {
        req->major = 1;
        req->minor = 1;
    } 
    else 
    {
        return -1;
    }

    req->response_code = atoi(number);
    if (!http_valid_response_code(req->response_code)) 
    {
        return -1;
    }

    req->response_code_line.len = strlen(readable);
    req->response_code_line.data = string_xxxpdup(req->mempool, 
        (uchar_t *)readable, req->response_code_line.len);
    if (!req->response_code_line.data)
    {
        return -1;
    }

    return 0;
}

static int http_parse_request_line(http_req_t *req, char *line)
{
    char* method = strsep(&line, " ");
    if (NULL == line)
    {
        return -1;
    }

    char* uri = strsep(&line, " ");
    if (NULL == line)
    {
        return -1;
    }

    char* version = strsep(&line, " ");
    if (NULL != line)
    {
        return -1;
    }

    if (0 == strcmp(method, "GET")) 
    {
        req->type = SHS_HTTP_REQ_TYPE_GET;
    } 
    else if (0 == strcmp(method, "POST")) 
    {
        req->type = SHS_HTTP_REQ_TYPE_POST;
    } 
    else if (0 == strcmp(method, "HEAD")) 
    {
        req->type = SHS_HTTP_REQ_TYPE_HEAD;
    } 
    else 
    {
        return -1;
    }

    if (0 == strcmp(version, "HTTP/1.0")) 
    {
        req->major = 1;
        req->minor = 0;
    } 
    else if (0 == strcmp(version, "HTTP/1.1")) 
    {
        req->major = 1;
        req->minor = 1;
    } 
    else 
    {
        return -1;
    }

    req->uri.len = strlen(uri);
    req->uri.data = string_xxxpdup(req->mempool, 
        (uchar_t *)uri, req->uri.len);
    if (!req->uri.data)
    {
        return -1;
    }

    return 0;
}

static char *strsep(char **s, const char *del)
{
    char *d, *tok;
    assert(strlen(del) == 1);

    if (!s || !*s)
    {
        return NULL;
    }

    tok = *s;
    d = strstr(tok, del);
    if (d) 
    {
        *d = '\0';
        *s = d + 1;
    }
    else
    {
        *s = NULL;
    }

    return tok;
}

static bool http_header_is_valid_value(const char *value)
{
    const char *p = value;

    while ((p = strpbrk(p, "\r\n")) != NULL) 
    {
        p += strspn(p, "\r\n");
        if (*p != ' ' && *p != '\t')
        {
            return false;
        }
    }

    return true;
}

int http_add_input_header(http_req_t *req, const std::string& key, 
    const std::string& value)
{
    if (key.find('\r') != std::string::npos 
        || key.find('\n') != std::string::npos) 
    {
        return -1;
    }
    
    if (!http_header_is_valid_value(value.c_str())) 
    {
        return -1;
    }

    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 == req->input_headers[i].key.len 
            && 0 == req->input_headers[i].value.len)
        {
            req->input_headers[i].key.len = key.size();
            req->input_headers[i].key.data = string_xxxpdup(req->mempool, 
                (uchar_t *)key.c_str(), key.size());

            req->input_headers[i].value.len = value.size();
            req->input_headers[i].value.data = string_xxxpdup(req->mempool, 
                (uchar_t *)value.c_str(), value.size());

            break;
        }
    }

    return 0;
}

int http_add_output_header(http_req_t *req, const std::string& key, 
    const std::string& value)
{
    if (key.find('\r') != std::string::npos 
        || key.find('\n') != std::string::npos) 
    {
        return -1;
    }
    
    if (!http_header_is_valid_value(value.c_str())) 
    {
        return -1;
    }

    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 == req->output_headers[i].key.len 
            && 0 == req->output_headers[i].value.len)
        {
            req->output_headers[i].key.len = key.size();
            req->output_headers[i].key.data = string_xxxpdup(req->mempool, 
                (uchar_t *)key.c_str(), key.size());

            req->output_headers[i].value.len = value.size();
            req->output_headers[i].value.data = string_xxxpdup(req->mempool, 
                (uchar_t *)value.c_str(), value.size());

            break;
        }
    }

    return 0;
}

static bool http_find_header(const http_header_t headers[], 
    const std::string& key, std::string* value)
{
    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 != headers[i].key.len 
            && 0 == strncasecmp((const char *)headers[i].key.data, 
            key.c_str(), key.size()))
        {
            value->assign((const char *)headers[i].value.data);

            return true;
        }
    }

    return false;
}

bool http_exist_header(const http_header_t headers[], const std::string& key)
{
    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 != headers[i].key.len 
            && 0 == strncasecmp((const char *)headers[i].key.data, 
            key.c_str(), key.size()))
        {
            return true;
        }
    }

    return false;
}

static enum HTTP_READ_STATUS http_parse_firstline(http_req_t *req) 
{
    enum HTTP_READ_STATUS status = ALL_DATA_READ;

    char* line = buffer_readline(req);
    if (NULL == line)
    {
        return MORE_DATA_EXPECTED;
    }

    switch (req->kind) 
    {
    case SHS_HTTP_KIND_REQUEST:
        if (http_parse_request_line(req, line) < 0)
        {
            status = DATA_CORRUPTED;
        }
        break;

    case SHS_HTTP_KIND_RESPONSE:
        if (http_parse_response_line(req, line) < 0)
        {
            status = DATA_CORRUPTED;
        }
        break;

    default:
        status = DATA_CORRUPTED;
    }

    return status;
}

static char *buffer_readline(http_req_t *req)
{
    int len = buffer_size(req->in);
    char *data = (char *)req->in->pos;
    char *line = NULL;

    int i = 0;
    for (i = 0; i < len; i++) 
    {
        if (data[i] == '\r' || data[i] == '\n')
        {
            break;
        }
    }

    if (i == len)
    {
        return NULL;
    }

    line = (char *)pool_calloc(req->mempool, i + 1);
    if (!line)
    {
        return NULL;
    }

    memcpy(line, data, i);
    line[i] = '\0';

    // Some protocols terminate a line with '\r\n', so check for that
    if (i < len - 1) 
    {
        char fch = data[i], sch = data[i + 1];
        if ((sch == '\r' || sch == '\n') && sch != fch)
        {
            i += 1;
        }
    }

    req->in->pos += (i + 1);

    return line;
}

static int http_append_to_last_input_header(http_req_t *req, 
    const std::string& line)
{
    if (0 == req->input_headers[0].key.len 
        && 0 == req->input_headers[0].value.len)
    {
        return -1;
    }

    int last_idx = 0;

    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 == req->input_headers[i].key.len 
            && 0 == req->input_headers[i].value.len)
        {
            last_idx = i - 1;

            break;
        }
    }

    int len = req->input_headers[last_idx].value.len + line.size() + 1;
    uchar_t *data = (uchar_t *)pool_calloc(req->mempool, len);
    if (!data)
    {
        SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
            << " pool_calloc() failed.";

        return -1;
    }

    memory_memcpy(data, req->input_headers[last_idx].value.data, 
        req->input_headers[last_idx].value.len);
    memory_memcpy(data + req->input_headers[last_idx].value.len, 
        line.c_str(), line.size());
    data[len - 1] = '\0';
    req->input_headers[last_idx].value.len = len;
    req->input_headers[last_idx].value.data = data;

    return 0;
}

static enum HTTP_READ_STATUS http_parse_headers(http_req_t *req) 
{
    char *line = NULL;
    enum HTTP_READ_STATUS status = MORE_DATA_EXPECTED;

    while (NULL != (line = buffer_readline(req))) 
    {
        char *skey, *svalue;

        if (*line == '\0') 
        {
            // done
            status = ALL_DATA_READ;

            break;
        }

        if (*line == ' ' || *line == '\t') 
        {
            if (http_append_to_last_input_header(req, line) < 0)
            {
                return DATA_CORRUPTED;
            }

            continue;
        }

        svalue = line;
        skey = strsep(&svalue, ":");
        if (NULL == svalue)
        {
            return DATA_CORRUPTED;
        }

        svalue += strspn(svalue, " ");

        if (http_add_input_header(req, skey, svalue) < 0)
        {
            return DATA_CORRUPTED;
        }
    }

    return status;
}

static int http_get_body_length(http_req_t *req)
{
    std::string content_length;
    std::string connection;

    bool exist_content_length = http_find_header(req->input_headers, 
        "Content-Length", &content_length);
    bool exist_connection = http_find_header(req->input_headers, 
        "Connection", &connection);
        
    if (!exist_content_length && !exist_connection) 
    {
        return -1;
    } 
    else if (!exist_content_length 
        && strncasecmp(connection.c_str(), "Close", 5) != 0) 
    {
        return -1;
    } 
    else if (!exist_content_length) 
    {
        return -1;
    } 
    else 
    {
        char *endp;
        auto ntoread = strtoll(content_length.c_str(), &endp, 10);
        if (content_length[0] == '\0' || *endp != '\0' || ntoread <= 0) 
        {
            return -1;
        }

        req->ntoread = ntoread;
    }
        
    return 0;
}

static void http_get_body(http_conn_t *hc, http_req_t *req)
{
    if (req->kind == SHS_HTTP_KIND_REQUEST 
        && req->type != SHS_HTTP_REQ_TYPE_POST) 
    {
        http_conn_done(hc, req);

        return;
    }

    hc->status = HTTP_STATUS_READING_BODY;

    std::string xfer_enc;
    if (http_find_header(req->input_headers, "Transfer-Encoding", &xfer_enc)
        && 0 == strncasecmp(xfer_enc.c_str(), "chunked", 7)) 
    {
        http_conn_fail(hc, HTTP_CODE_INVALID_BODY);

        return;
    } 
    else 
    {
        if (http_get_body_length(req) < 0) 
        {
            http_conn_fail(hc, HTTP_CODE_INVALID_BODY);

            return;
        }
    }

    http_read_body(hc, req);
}

static void http_read_firstline(http_conn_t *hc, http_req_t *req)
{
    conn_t *c = hc->c;
    event_t *rev = c->read;

    auto ret = http_parse_firstline(req);
    if (DATA_CORRUPTED == ret) 
    {
        http_conn_fail(hc, HTTP_CODE_INVALID_HEADER);

        return;
    }
    else if (MORE_DATA_EXPECTED == ret) 
    {
        event_handle_read(c->ev_base, rev, 0);

        return;
    }

    hc->status = HTTP_STATUS_READING_HEADERS;
    http_read_header(hc, req);
}

static void http_read_header(http_conn_t *hc, http_req_t *req)
{
    conn_t *c = hc->c;
    event_t *rev = c->read;

    auto res = http_parse_headers(req);
    if (DATA_CORRUPTED == res) 
    {
        http_conn_fail(hc, HTTP_CODE_INVALID_HEADER);

        return;
    } 
    else if (MORE_DATA_EXPECTED == res) 
    {
        event_handle_read(c->ev_base, rev, 0);

        return;
    }

    switch (req->kind) 
    {
    case SHS_HTTP_KIND_REQUEST:
        http_get_body(hc, req);
        break;

    case SHS_HTTP_KIND_RESPONSE:
        if (HTTP_NOCONTENT == req->response_code 
            || HTTP_NOTMODIFIED == req->response_code
            || (req->response_code >= 100 && req->response_code < 200)) 
        {
            http_conn_done(hc, req);
        } 
        else 
        {
            http_get_body(hc, req);
        }
        break;

    default:
        http_conn_fail(hc, HTTP_CODE_INVALID_HEADER);
        break;
    }
}

void http_conn_set_connect_timeout_ms(http_conn_t *hc, 
    int timeout_in_ms)
{
    hc->timeout_connect = timeout_in_ms;
}

void http_conn_set_recv_timeout_ms(http_conn_t *hc, 
    int timeout_in_ms)
{
    hc->timeout_recv = timeout_in_ms;
}

void http_conn_set_retries(http_conn_t *hc, int retry_max)
{
    hc->retry_max = retry_max;
}

static int http_conn_connect(http_conn_t *hc)
{
    conn_t *c = hc->c;

    if (c->fd != -1) 
    {
        close(c->fd);
        c->fd = -1;
    }

    hc->flags |= HTTP_FLAGS_OUTGOING;
    hc->status = HTTP_STATUS_CONNECTING;
    
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd) 
    {
        return SHS_ERROR;
    }

    if (-1 == conn_nonblocking(fd))
    {
        close(fd);

        return SHS_ERROR;
    }

    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));

    conn_set_default(c, fd);
    c->recv = shs_recv;
    c->send = shs_send;
    c->ev_base = hc->base;
    c->ev_timer = hc->timer;

    if (-1 == event_add_conn(c->ev_base, c)) 
    {
        close(c->fd);
        c->fd = -1;

        return SHS_ERROR;
    }

    errno = 0;
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(hc->port);
    sin.sin_addr.s_addr = inet_addr(hc->host);
    if (connect(c->fd, (const struct sockaddr *)&sin, 
        sizeof(struct sockaddr_in)) == -1) 
    {
        if (errno == SHS_EINPROGRESS)
        {
            return SHS_AGAIN;
        }

        close(c->fd);
        c->fd = -1;

        return SHS_ERROR;
    }

    c->write->ready = SHS_TRUE;

    hc->status = HTTP_STATUS_IDLE;

    return SHS_OK;
}

int http_make_request(http_req_t *req, http_cmd_type type, 
    const std::string& uri, const std::string& body)
{
    req->in = buffer_create(req->mempool, CONN_DEFAULT_RCVBUF);
    req->out = buffer_create(req->mempool, HEADER_SZ);
    if (!req->in || !req->out)
    {
        SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
            << " buffer_create() failed.";

        return -1;
    }

    req->kind = SHS_HTTP_KIND_REQUEST;
    req->type = type;
    req->major = 1;
    req->minor = 0;
    req->userdone = false;
    req->uri.len = uri.size();
    req->uri.data = string_xxxpdup(req->mempool, 
        (uchar_t *)uri.c_str(), req->uri.len);
    if (!req->uri.data)
    {
        return -1;
    }

    if (SHS_HTTP_REQ_TYPE_POST == type && !body.empty())
    {
        req->output_body.len = body.size();
        req->output_body.data = (uchar_t *)pool_calloc(req->mempool,
            req->output_body.len);
        if (!req->output_body.data)
        {
            SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                << " pool_calloc() failed.";
        
            return -1;
        }
        memory_memcpy(req->output_body.data, body.c_str(),
            req->output_body.len);
    }

    int ret = http_conn_connect(req->hc);
    if (SHS_ERROR == ret)
    {
        return ret;
    }
    else if (SHS_OK == ret)
    {
        http_send_request(req->hc);
        
        return ret;
    }

    conn_t *c = req->hc->c;
    c->conn_data = req->hc;
    event_t *rev = c->read;
    event_t *wev = c->write; 
    rev->handler = event_process_handler;
    wev->handler = event_process_handler;

    req->hc->write_event_handler = conn_wait_connect_handler;

    event_timer_add(c->ev_timer, wev, req->hc->timeout_connect);

    return SHS_OK;
}

static void http_send_done(http_conn_t *hc, http_req_t *req)
{
    hc->req = NULL;

    bool need_close = 
        (0 == req->minor && !http_is_connection_keepalive(req->input_headers)) 
        || http_is_connection_close(req->flags, req->input_headers)
        || http_is_connection_close(req->flags, req->output_headers);

    if (1 == req->minor && FLAGS_disable_http_keepalive) 
    {
        need_close = true;
    }

    http_request_free(req);

    if (need_close)
    {
        http_conn_free(hc);

        return;
    }

    if (http_get_request_with_connection(hc) < 0)
    {
        http_conn_free(hc);
    }
}

static void http_send_error(http_req_t *req, int error, 
    const std::string& reason)
{
#define ERR_FORMAT "<HTML><HEAD>\n" \
        "<TITLE>%d %s</TITLE>\n" \
        "</HEAD><BODY>\n" \
        "<H1>Method Not Implemented</H1>\n" \
        "Invalid method in request<P>\n" \
        "</BODY></HTML>\n"

    http_response_code(req, error, reason);

    char buf[HEADER_SZ] = {0};
    sprintf(buf, ERR_FORMAT, error, reason.c_str());

    http_send_page(req, buf);
#undef ERR_FORMAT
}

static void http_send(http_req_t *req, const std::string& data)
{
    req->userdone = true;

    if (!data.empty())
    {
        req->output_body.len = data.size();
        req->output_body.data = (uchar_t *)pool_calloc(req->mempool,
            req->output_body.len);
        if (!req->output_body.data)
        {
            SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                << " pool_calloc() failed.";
        }
        memory_memcpy(req->output_body.data, data.c_str(), data.size());
    }

    http_make_header(req);

    req->hc->conn_cb = http_send_done;
    http_write_buffer(req->hc); 
}

void http_send_reply(http_req_t *req, int code, const std::string& reason, 
    const std::string& data)
{
    http_conn_t *hc = req->hc;
    if (!hc)
    {
        http_request_free(req);

        return;
    }

    http_response_code(req, code, reason);
    http_send(req, data);
}

static void http_response_code(http_req_t *req, int code, 
    const std::string& reason)
{
    req->kind = SHS_HTTP_KIND_RESPONSE;
    req->response_code = code;
    req->response_code_line.len = reason.size();
    req->response_code_line.data = string_xxxpdup(req->mempool, 
        (uchar_t *)reason.c_str(), req->response_code_line.len);
    if (!req->response_code_line.data)
    {
        SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
            << " string_xxxpdup() failed.";
    }
}

static void http_send_page(http_req_t *req, const std::string& data)
{
    if (!req->major || !req->minor) 
    {
        req->major = 1;
        req->minor = 1;
    }
    
    if (req->kind != SHS_HTTP_KIND_RESPONSE)
    {
        http_response_code(req, 200, "OK");
    }

    http_add_output_header(req, "Content-Type", "text/html");
    http_add_output_header(req, "Connection", "close");

    http_send(req, data);
}

static const char uri_chars[256] = {
    /* 0 */
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 0, 0, 0, 0, 0, 0,
    /* 64 */
    0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 1, 0,
    /* 128 */
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    /* 192 */
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};

#define CHAR_IS_UNRESERVED(c)           \
    (uri_chars[(unsigned char)(c)])

static std::string http_uriencode(const std::string& value, 
    bool space_as_plus)
{
    std::string result;
    for (size_t i = 0; i < value.length(); i++) 
    {
        if (CHAR_IS_UNRESERVED(value[i])) 
        {
            result.append(1, value[i]);
        } 
        else if (value[i] == ' ' && space_as_plus) 
        {
            result.append(1, '+');
        } 
        else 
        {
            char buf[30];
            snprintf(buf, sizeof(buf), "%%%02X", 
                static_cast<unsigned char>(value[i]));
            result.append(buf);
        }
    }

    return result;
}

std::string http_encode_uri(const std::string& value)
{
    return http_uriencode(value, false);
}

static void http_decode_uri_internal(const std::string& value, 
    std::string* decode_value, bool decode_plus)
{
    for (size_t i = 0; i < value.length(); i++) 
    {
        auto c = value[i];
        if (c == '+' && decode_plus) 
        {
            c = ' ';
        } 
        else if (c == '%' && isxdigit(value[i+1]) && isxdigit(value[i+2])) 
        {
            char tmp[3];
            tmp[0] = value[i+1];
            tmp[1] = value[i+2];
            tmp[2] = '\0';
            c = static_cast<char>(strtol(tmp, NULL, 16));
            i += 2;
        }
        decode_value->append(1, c);
    }
}

std::string http_decode_uri(const std::string& value, bool decode_plus)
{
    std::string result;
    http_decode_uri_internal(value, &result, decode_plus);

    return result;
}

void http_parse_query(const std::string& uri, HttpQuery* output)
{
    auto pos = uri.find_first_of('?');
    if (pos == std::string::npos)
    {
        return;
    }

    std::string query(uri.substr(pos + 1));

    std::vector<std::string> result;
    boost::split(result, query, boost::is_any_of("&"));

    for (auto it = result.begin(); it != result.end(); ++it)
    {
        std::string& kv = *it;
        pos = kv.find('=');
        if (pos == std::string::npos)
        {
            break;
        }

        auto key = kv.substr(0, pos);
        auto value = kv.substr(pos+1);
        std::string decoded_value;
        http_decode_uri_internal(value, &decoded_value, 0);
        output->insert(std::make_pair(key, decoded_value));
    }
}

static void http_handle_request(HTTP_CODE ec, 
    http_req_t *req, void *data)
{
    http_srv_t* http = (http_srv_t *)data;

    if (0 == req->uri.len) 
    {
        if (HTTP_STATUS_DISCONNECTED == req->hc->status) 
        { 
            http_conn_fail(req->hc, HTTP_CODE_READ_EOF); 
        } 
        else 
        {
            http_send_error(req, HTTP_BADREQUEST, "Bad Request");
        }

        return;
    }

    req->userdone = false;

    if ((0 == string_strncmp("/status", (const char *)req->uri.data, 7) 
        || 0 == string_strncmp("/stats", (const char *)req->uri.data, 6))
        && http->stat_cb)
    {
        http->stat_cb(ec, req, http->data);

        return;
    }

    if (http->req_cb) 
    {
        http->req_cb(ec, req, http->data);
        req->uri = string_null;

        return;
    } 
    else 
    {
#define ERR_FORMAT "<html><head>" \
            "<title>404 Not Found</title>" \
            "</head><body>" \
            "<h1>Not Found</h1>" \
            "<p>The requested URL %s was not found on this server.</p>"\
            "</body></html>\n"

        auto escaped_html = http_htmlescape((const char *)req->uri.data);

        char buf[HEADER_SZ] = {0};
        sprintf(buf, ERR_FORMAT, escaped_html.c_str());

        http_response_code(req, HTTP_NOTFOUND, "Not Found");

        http_send_page(req, buf);
#undef ERR_FORMAT
    }
}

static void http_get_request(http_srv_t *http, conn_t *c, 
    char *host, int port)
{
    pool_t *mempool = pool_create(CONN_DEFAULT_POOL_SIZE, 
        CONN_DEFAULT_POOL_SIZE);
    if (!mempool)
    {
        conn_release(c);
        conn_pool_free_connection(http->conn_pool, c);

        return;
    }

    http_conn_t *hc = (http_conn_t *)pool_calloc(mempool, 
        sizeof(http_conn_t));
    if (!hc)
    {
        conn_release(c);
        conn_pool_free_connection(http->conn_pool, c);
        pool_destroy(mempool);

        return;
    }

    ProcessStats::SetServerConns(++http->connections);

    hc->mempool = mempool;
    hc->http_srv = http;
    hc->connpool = http->conn_pool;
    hc->c = c;
    strcpy(hc->host, host);
    hc->port = port;
    hc->flags = HTTP_FLAGS_INCOMING;
    hc->status = HTTP_STATUS_READING_FIRSTLINE;
    hc->timeout_recv = CONN_TIME_OUT;
    hc->timeout_send = CONN_TIME_OUT;

    if (http_get_request_with_connection(hc) < 0)
    {
        http_conn_free(hc);
    }
}

void http_accept_handler(event_t *ev)
{
    conn_t *lc = (conn_t *)ev->data;
    http_srv_t *http = (http_srv_t *)lc->conn_data;
    conn_pool_t *conn_pool = http->conn_pool;

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    int nfd = accept4(lc->fd, reinterpret_cast<struct sockaddr*>(&addr), 
        &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (-1 == nfd) 
    {
        return;
    }

    conn_t *nc = conn_pool_get_connection(conn_pool);
    if (!nc)
    {
        close(nfd);

        return;
    } 

    conn_set_default(nc, nfd);
    nc->recv = shs_recv;
    nc->send = shs_send;
    nc->ev_base = http->base;
    nc->ev_timer = http->timer;
    nc->write->ready = SHS_FALSE;
    
    char ntop[HOST_LEN];
    char strport[PORT_LEN];
    getnameinfo(reinterpret_cast<struct sockaddr *>(&addr), addrlen, 
        ntop, sizeof(ntop), strport, sizeof(strport), 1 | 2);

    http_get_request(http, nc, ntop, atoi(strport));
}

static void http_request_free(http_req_t *req)
{
    if (req->flags & HTTP_REQ_FLAGS_INCOMING)
    {
        ProcessStats::DecServerReqs();
    }

    req->data = NULL;
    req->cb = NULL;
    req->hc = NULL;
    req->in = NULL;
    req->out = NULL;
    req->input_body = string_null;
    req->output_body = string_null;
    req->uri = string_null;
    req->response_code_line = string_null;
    http_init_headers(req);

    if (req->mempool)
    {
        pool_destroy(req->mempool);
    }
}

void http_init_headers(http_req_t *req)
{
    for (int i = 0; i < HEADER_NUM; i++)
    {
        req->input_headers[i].key = string_null;
        req->input_headers[i].value = string_null;
        req->output_headers[i].key = string_null;
        req->output_headers[i].value = string_null;
    }
}

static int http_get_request_with_connection(http_conn_t *hc)
{
    pool_t *mempool = pool_create(CONN_DEFAULT_POOL_SIZE, 
        CONN_DEFAULT_POOL_SIZE);
    if (!mempool)
    {
        return -1;
    }

    http_req_t *req = (http_req_t *)pool_calloc(mempool, 
        sizeof(http_req_t));
    if (!req)
    {
        pool_destroy(mempool);

        return -1;
    }

    req->mempool = mempool;
    req->data = hc->http_srv;
    req->cb = http_handle_request;
    req->hc = hc;
    req->flags = HTTP_REQ_FLAGS_INCOMING;
    req->kind = SHS_HTTP_KIND_REQUEST;
    req->userdone = true;
    req->input_body = string_null;
    req->output_body = string_null;
    req->uri = string_null;
    req->response_code_line = string_null;
    http_init_headers(req);

    req->in = buffer_create(req->mempool, CONN_DEFAULT_RCVBUF);
    req->out = buffer_create(req->mempool, HEADER_SZ);
    if (!req->in || !req->out)
    {
        return -1;
    }

    ProcessStats::IncServerReqs();

    hc->status = HTTP_STATUS_READING_FIRSTLINE;
    hc->req = req;

    conn_t *c = hc->c;
    event_t *rev = c->read;
    event_t *wev = c->write; 
    rev->handler = event_process_handler;
    wev->handler = event_process_handler;
    hc->read_event_handler = conn_read_handler;

    c->conn_data = hc;

    if (event_handle_read(c->ev_base, rev, 0) < 0)
    {
        return -1;
    }

    return 0;
}

static void event_process_handler(event_t *ev)
{
    conn_t *c = (conn_t *)ev->data;
    http_conn_t *hc = (http_conn_t *)c->conn_data;

    if (ev->write)
    {
        if (hc->write_event_handler)
        {
            hc->write_event_handler(hc);
        }
    }
    else
    {
        if (hc->read_event_handler)
        {
            hc->read_event_handler(hc);
        }
    }
}

static void conn_read_handler(http_conn_t *hc)
{
    http_req_t *req = hc->req;

    if (hc->c->read->timedout)
    {
        http_conn_fail(hc, HTTP_CODE_READ_TIMEOUT);

        return;
    }

    if (hc->c->read->timer_set)
    {
        event_timer_del(hc->c->ev_timer, hc->c->read);
    }

    int n = conn_recv(req);
    if (0 == n && 0 != buffer_size(req->in))
    {
        goto recv_done;        
    }
    else if (SHS_AGAIN == n && 0 == buffer_size(req->in))
    {
        conn_t *c = hc->c;
        event_t *rev = c->read;
        event_handle_read(c->ev_base, rev, 0);

        return;
    }
    else if (-1 == n)
    {
        http_conn_fail(hc, HTTP_CODE_READ_EOF);

        return;
    }
    else if (0 == n)
    {
        hc->status = HTTP_STATUS_DISCONNECTED;
        http_conn_done(hc, req);

        return;
    }

recv_done:
    switch (hc->status) 
    {
    case HTTP_STATUS_READING_FIRSTLINE:
        http_read_firstline(hc, req);
        break;

    case HTTP_STATUS_READING_HEADERS:
        http_read_header(hc, req);
        break;

    case HTTP_STATUS_READING_BODY:
        http_read_body(hc, req);
        break;

    default:
        break;
    }
}

static int conn_recv(http_req_t *req)
{
    int n = 0;
    size_t blen = 0;
    conn_t *c = req->hc->c;

    while (1)
    {
        buffer_shrink(req->in);

        blen = buffer_free_size(req->in);
        if (!blen)
        {
            return buffer_size(req->in);
        }

        n = c->recv(c, req->in->last, blen);
        if (n > 0)
        {
            req->in->last += n;

            continue;
        }
        else
        {
            return n;
        }
    }

    return 0;
}

static const struct 
{
    int response_code;
    const char* reason_phrase;
} kStatusInfo[] =
{
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information"},
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 307, "Temporary Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Request Entity Too Large" },
    { 414, "Request-URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Requested Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
    { 0,   NULL },
};

const char* get_reason_phrase(int response_code)
{
    int i = 0;

    while (kStatusInfo[i].response_code)
    {
        if (kStatusInfo[i].response_code == response_code)
        {
            return kStatusInfo[i].reason_phrase;
        }

        i++;
    }

    return NULL;
}

} // namespace shs
