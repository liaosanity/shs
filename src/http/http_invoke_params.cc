#include <string.h>
#include <string>
#include <boost/format.hpp>

#include "http_invoke_params.h"

namespace shs 
{

HttpInvokeParams::HttpInvokeParams()
    : InvokeParams()
    , type_(SHS_HTTP_REQ_TYPE_GET)
    , major_(1)
    , minor_(0)
{
}

void HttpInvokeParams::set_header(const std::string& key, 
    const std::string& value)
{
    if (strcasecmp(key.c_str(), "Set-Cookie"))
    {
        headers_[key] = value;
        return ;
    }

    std::map<std::string, std::string>::iterator it = headers_.find(key);
    if (it == headers_.end())
    {
        uint16_t count = 0;
        it = headers_.insert(std::make_pair(key, 
            std::string(reinterpret_cast<const char*>(&count), 
            sizeof count))).first;
    }
    *(reinterpret_cast<uint16_t*>(const_cast<char*>(it->second.data()))) += 1;

    uint16_t length = static_cast<uint16_t>(value.length());
    it->second.append(reinterpret_cast<const char*>(&length), sizeof length);
    it->second.append(value);
}    

std::string HttpInvokeParams::get_protocol() const
{
    return boost::str(boost::format("%1%.%2%") % major_ % minor_);
}

std::string HttpInvokeParams::get_header(const std::string& key)
{
    std::map<std::string, std::string>::iterator iter = headers_.find(key);
    if (iter == headers_.end())
    {
        return "";
    }

    return iter->second;
}

} // namespace shs
