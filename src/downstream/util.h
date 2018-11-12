#pragma once

#include "downstream/err_code.h"

namespace shs 
{
namespace downstream 
{

inline unsigned int CalcHash(const char *key, int len)
{
    unsigned int m = 0x5bd1e995;
    unsigned int h = 0x9747b28c ^ len;
    unsigned int t;
    const char *s = key;

    while (len >= 4) 
    {
        t = *(unsigned int *)s;
        s += 4; 
        len -= 4;
        t *= m; 
        t ^= t >> 24; t *= m;
        h *= m; 
        h ^= t;
    }

    switch (len) 
    {
    case 3: h ^= s[2] << 16;
    case 2: h ^= s[1] << 8;
    case 1: h ^= s[0]; h *= m;
    }

    h ^= h >> 13; 
    h *= m; 
    h ^= h >> 15;

    return h;
}

inline std::string ErrCodeToString(ErrCode ec)
{
    switch (ec)
    {
    case OK:
        return "OK";
    case E_BAD_REQUEST:
        return "Bad Request";
    case E_CONN_REFUSED:
        return "Connection refused";
    case E_CONN_FAILED:
        return "Connection failed";
    case E_CONN_CLOSED:
        return "Connection closed";
    case E_BAD_RESPONSE:
        return "Bad Response(not 200)";
    case E_REQUEST_TIMEOUT:
        return "Request timeout";
    case E_CREATESERVER_FAIL:
        return "Create downstream server fail";
    case E_NO_QUERYCHANCE:
        return "No query change for re-excute";
    case E_SELECT_SERVER_FAIL:
        return "Select server from group fail";
    case E_OTHER_FAIL:
        return "Other fail";
    case E_HTTP_INVALID_HEADER:
        return "Http invalid header";
    case E_HTTP_INVALID_BODY:
        return "Http invalid body";
    case E_HTTP_READ_TIMEOUT:
        return "Http read timeout";
    case E_HTTP_READ_EOF:
        return "Http read eof";
    case E_HTTP_WRITE_TIMEOUT:
        return "Http write timeout";
    case E_HTTP_WRITE_EOF:
        return "Http write eof";
    case E_HTTP_CONNECT_TIMEOUT:
        return "Http connect timeout";
    case E_HTTP_CONNECT_FAIL:
        return "Http connect fail";
    case E_HTTP_INVALID_SOCKET:
        return "Http invalid socket";
    case E_HTTP_CALLER_ERROR:
        return "Http caller error";

    default:
        return "Unknown";
    }
}

} // namespace downstream
} // namespace shs
