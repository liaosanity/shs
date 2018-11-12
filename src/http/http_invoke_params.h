#ifndef HTTP_INVOKE_PARAMS_H
#define HTTP_INVOKE_PARAMS_H

#include <stdint.h>
#include <string>
#include <map>

#include "http/http.h"
#include "boost/shared_ptr.hpp"

#include "invoke_params.h"

namespace shs 
{

class HttpInvokeParams : public InvokeParams 
{
public:
    HttpInvokeParams();

    void set_type(SHS_HTTP_REQ_TYPE type)
    {
        type_ = type;
    }

    void set_protocol(uint8_t major, uint8_t minor)
    {
        major_ = major;
        minor_ = minor;
    }

    void set_header(const std::string& key, const std::string& value);

    SHS_HTTP_REQ_TYPE get_type() const { return type_; } 
    std::string get_protocol() const;
    uint8_t get_major() const { return major_; }
    uint8_t get_minor() const { return minor_; } 
    const std::map<std::string, std::string>& get_headers() const { return headers_; }
    std::string get_header(const std::string& key);

    std::string uri() const { return uri_; }
    void set_uri(const std::string& uri)
    {
        uri_ = uri;
    }

private:
    SHS_HTTP_REQ_TYPE type_;
    uint8_t major_;
    uint8_t minor_;
    std::map<std::string, std::string> headers_;
    std::string uri_;
};

} // namespace shs

#endif // HTTP_INVOKE_PARAMS_H
