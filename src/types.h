#pragma once

#include <stdio.h>
#include <stdint.h>
#include <map>
#include <string>
#include <tr1/functional>
#include <boost/shared_ptr.hpp>

namespace shs 
{

struct ErrorCode 
{
    enum type 
    {
        OK = 0,
        E_INVALID_MODULE = -1,
        E_INVALID_METHOD = -2,
        E_INVALID_TIMEOUT = -3,
        E_INVOKE_FAILED = -4,
        E_INVOKE_TIMEOUT = -5,
        E_SERVICE_BUSY = -6,
        E_INTERNAL = -7,
        E_NETWORK = -8,
        E_BAD_REQUEST = -9,
        E_REQUEST_EXPIRE = -10,

        E_CODEC_INVALID_LENGTH = -1001,
        E_CODEC_INVALID_TAG = -1002,
        E_CODEC_INVALID_MAGIC = -1003,
        E_CODEC_PARSE_FAIL = -1004,
        E_CONNECTION_READ_EOF = -1005,
        E_CONNECTION_WRITE_EOF = -1006,
        E_CONNECTION_READ_TIMEOUT = -1007,
        E_CONNECTION_WRITE_TIMEOUT = -1008,
    };
};

inline std::string ErrorCodeToString(ErrorCode::type ec)
{
    switch (ec)
    {
    case ErrorCode::OK:
        return "OK";
    case ErrorCode::E_INVALID_MODULE:
        return "Invalid module";
    case ErrorCode::E_INVALID_METHOD:
        return "Invalid method";
    case ErrorCode::E_INVALID_TIMEOUT:
        return "Invalid timeout";
    case ErrorCode::E_INVOKE_FAILED:
        return "Invoke failed";
    case ErrorCode::E_INVOKE_TIMEOUT:
        return "Invoke timeout";
    case ErrorCode::E_SERVICE_BUSY:
        return "Service busy";
    case ErrorCode::E_INTERNAL:
        return "Internal error";
    case ErrorCode::E_NETWORK:
        return "Network error";

    case ErrorCode::E_CODEC_INVALID_LENGTH:
        return "Codec Invalid length";
    case ErrorCode::E_CODEC_INVALID_TAG:
        return "Codec Invalid tag";
    case ErrorCode::E_CODEC_INVALID_MAGIC:
        return "Codec Invalid magic";
    case ErrorCode::E_CODEC_PARSE_FAIL:
        return "Codec Parse fail";
    case ErrorCode::E_CONNECTION_READ_EOF:
        return "Connection Read eof";
    case ErrorCode::E_CONNECTION_WRITE_EOF:
        return "Connection Write eof";
    case ErrorCode::E_CONNECTION_READ_TIMEOUT:
        return "Connection Read timeout";
    case ErrorCode::E_CONNECTION_WRITE_TIMEOUT:
        return "Connection Write timeout";

    default:
        return "Unknown";
    }
}

typedef struct invoke_result_s 
{
    invoke_result_s() : ec(false), msg(false), results(false) {}
    bool ec;
    bool msg;
    bool results;
} invoke_result_t;

class InvokeResult 
{
public:
    InvokeResult() : ec(0), msg("") {}
    virtual ~InvokeResult() throw() {}

    int32_t ec;
    std::string msg;
    std::map<std::string, std::string> results;

    invoke_result_t irset;

    void set_ec(const int32_t val) 
    {
        ec = val;
        irset.ec = true;
    }

    void set_msg(const std::string& val) 
    {
        msg = val;
        irset.msg = true;
    }

    void set_results(const std::map<std::string, std::string>& val) 
    {
        results = val;
        irset.results = true;
    }

    bool operator == (const InvokeResult & rhs) const
    {
        if (irset.ec != rhs.irset.ec)
        {
            return false;
        }
        else if (irset.ec && !(ec == rhs.ec))
        {
            return false;
        }

        if (irset.msg != rhs.irset.msg)
        {
            return false;
        }
        else if (irset.msg && !(msg == rhs.msg))
        {
            return false;
        }

        if (irset.results != rhs.irset.results)
        {
            return false;
        }
        else if (irset.results && !(results == rhs.results))
        {
            return false;
        }

        return true;
    }

    bool operator != (const InvokeResult &rhs) const 
    {
        return !(*this == rhs);
    }
};

enum SHS_HTTP_REQ_TYPE 
{
    SHS_HTTP_REQ_TYPE_GET,
    SHS_HTTP_REQ_TYPE_POST,
    SHS_HTTP_REQ_TYPE_HEAD
};

enum SHS_HTTP_REQUEST_KIND 
{
    SHS_HTTP_KIND_REQUEST,
    SHS_HTTP_KIND_RESPONSE
};

class InvokeParams;
typedef std::tr1::function<void(const InvokeResult&)> InvokeCompleteHandler;
typedef std::tr1::function<void(
    const std::map<std::string, std::string>&,
    const InvokeCompleteHandler&,
    boost::shared_ptr<InvokeParams>)> InvokeHandler;

} // namespace shs
