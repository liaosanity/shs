#pragma once

#include <string>
#include <vector>

#include "types.h"

#include "downstream/err_code.h"
#include "http/http.h"

namespace shs 
{
namespace downstream 
{

class Response
{
public:
    Response(http_req_t* rsp = NULL,
        const std::string& host = "", int retry_cnt = 0)
        : code_(0)
        , host_(host)
        , retry_cnt_(retry_cnt)
    {
        if (NULL != rsp)
        {
            code_ = rsp->response_code;
            body_.assign((const char *)rsp->input_body.data, 
                rsp->input_body.len);
        }
    }

    virtual ~Response() {}

    void set_code(int code)
    {
        code_ = code;
    }

    int code() const 
    { 
        return code_;
    }

    const std::string& host() const 
    { 
        return host_; 
    }

    int retry_count() const 
    {
        return retry_cnt_; 
    }

    const std::string& body() const 
    { 
        return body_; 
    }

private:
    int code_;
    int retry_cnt_;
    std::string host_;
    std::string body_;
};

typedef std::tr1::function<void(ErrCode, 
    boost::shared_ptr<Response>)> ResponseHandler;
typedef Response GetResponse;
typedef Response PostResponse;

} // namespace downstream
} // namespace shs
