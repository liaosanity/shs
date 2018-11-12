#pragma once

#include <string.h>
#include <map>
#include <vector>
#include <string>
#include <deque>
#include <set>
#include <tr1/functional>

#include "types.h"
#include "core/shs_types.h"
#include "core/shs_conn_pool.h"
#include "core/shs_conn.h"
#include "core/shs_string.h"

namespace shs 
{

#define HTTP_OK          200
#define HTTP_NOCONTENT   204
#define HTTP_MOVEPERM    301
#define HTTP_MOVETEMP    302
#define HTTP_NOTMODIFIED 304
#define HTTP_BADREQUEST  400
#define HTTP_NOTFOUND    404
#define HTTP_SERVUNAVAIL 503

#define HTTP_FLAGS_INCOMING     0x0001
#define HTTP_FLAGS_OUTGOING     0x0002
#define HTTP_FLAGS_CLOSEDETECT  0x0003
#define HTTP_REQ_FLAGS_INCOMING 0x0004

#define HOST_LEN      1024
#define PORT_LEN      32
#define HEADER_SZ     4096
#define HEADER_NUM    35
#define CONN_TIME_OUT 4500

enum HTTP_CODE 
{
    HTTP_CODE_OK = 0,
    HTTP_CODE_INVALID_HEADER = 1,
    HTTP_CODE_INVALID_BODY = 2,
    HTTP_CODE_READ_TIMEOUT = 3,
    HTTP_CODE_READ_EOF = 4,
    HTTP_CODE_WRITE_TIMEOUT = 5,
    HTTP_CODE_WRITE_EOF = 6,
    HTTP_CODE_CONNECT_TIMEOUT = 7,
    HTTP_CODE_CONNECT_FAIL = 8,
    HTTP_CODE_INVALID_SOCKET = 9,
    HTTP_CODE_CALLER_ERROR = 10
};

enum HTTP_READ_STATUS
{
    ALL_DATA_READ = 1,
    MORE_DATA_EXPECTED = 0,
    DATA_CORRUPTED = -1,
    REQUEST_CANCELED = -2
};

enum HTTP_CONN_STATUS 
{
    HTTP_STATUS_DISCONNECTED,
    HTTP_STATUS_CONNECTING,
    HTTP_STATUS_IDLE,
    HTTP_STATUS_READING_FIRSTLINE,
    HTTP_STATUS_READING_HEADERS,
    HTTP_STATUS_READING_BODY,
    HTTP_STATUS_READING_TRAILER,
    HTTP_STATUS_WRITING
};

typedef struct http_header_s http_header_t;
typedef struct http_srv_s http_srv_t;
typedef struct http_conn_s http_conn_t; 
typedef struct http_req_s http_req_t;

typedef std::map<std::string, std::string> HttpQuery;
typedef SHS_HTTP_REQ_TYPE http_cmd_type;
typedef SHS_HTTP_REQUEST_KIND http_request_kind;
typedef void (*event_handler_pt)(http_conn_t *);
typedef void (*http_conn_cb)(http_conn_t *, http_req_t *);
typedef void (*http_req_cb)(HTTP_CODE, http_req_t *, void *);

void http_send_reply(http_req_t *, int, const std::string&, 
    const std::string&);
int http_add_input_header(http_req_t *, 
    const std::string&, const std::string&);
int http_add_output_header(http_req_t *, 
    const std::string&, const std::string&);
void http_init_headers(http_req_t *);
bool http_exist_header(const http_header_t headers[], const std::string&);
const char* get_reason_phrase(int);
void http_conn_free(http_conn_t *);
void http_conn_set_recv_timeout_ms(http_conn_t *, int);
void http_conn_set_connect_timeout_ms(http_conn_t *, int);
void http_conn_set_retries(http_conn_t *, int);
int http_make_request(http_req_t *, http_cmd_type, 
    const std::string&, const std::string&);
std::string http_encode_uri(const std::string&);
std::string http_decode_uri(const std::string&, bool);
void http_parse_query(const std::string&, HttpQuery *);
void http_accept_handler(event_t *);

struct http_header_s
{
    string_t key;
    string_t value;
};

struct http_req_s
{
    http_conn_t *hc;
    buffer_t *in;
    buffer_t *out;
    pool_t *mempool;
    void *data;

    string_t uri; 
    string_t input_body;
    string_t output_body;
    string_t response_code_line;

    http_header_t input_headers[HEADER_NUM];
    http_header_t output_headers[HEADER_NUM];
    http_request_kind kind;
    http_cmd_type type;
    http_req_cb cb;

    int8_t major;
    int8_t minor;
    int flags;
    int response_code;
    int64_t ntoread;
    int64_t read_done;
    bool userdone;
};

struct http_conn_s 
{
    http_req_t *req;
    pool_t *mempool;
    http_srv_t *http_srv;
    conn_pool_t *connpool;
    conn_t *c;
    event_base_t *base;
    event_timer_t *timer;

    event_handler_pt read_event_handler;
    event_handler_pt write_event_handler;
    http_conn_cb conn_cb;
    
    char host[HOST_LEN];
    int port;
    int flags;
    int timeout_recv;
    int timeout_send;
    int timeout_connect;
    int retry_cnt;
    int retry_max;
    enum HTTP_CONN_STATUS status;
};

struct http_srv_s
{
    event_base_t *base;
    event_timer_t *timer;
    conn_pool_t *conn_pool;
    conn_t *c;
    http_req_cb stat_cb;
    http_req_cb req_cb;
    void *data;
    int connections;
};

} // namespace shs
