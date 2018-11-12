#pragma once

#include <map>
#include <vector>
#include <string>

#include "downstream/response.h"

namespace shs 
{ 
namespace downstream 
{ 

class Server;
class RequestContext;

class RequestUri
{
public:
    RequestUri(const std::string& base);
    ~RequestUri();

    void AddQuery(const std::map<std::string, std::string>& query);
    void AddQuery(const std::string& name, const std::string& value);
    std::string uri();

private:
    std::string base_uri_;
    std::map<std::string, std::string> query_;
};

class Request
{
    friend class RequestContext;
    typedef SHS_HTTP_REQ_TYPE HttpType;

public:
    Request(const std::string& uri, const std::string& body, 
        const std::string& hash, HttpType type);
    ~Request();

    void Execute(Server *server, const ResponseHandler& response_handler);
    void AddHeader(const std::string& key, const std::string& value);

    const std::string& uri() const { return uri_; }
    const std::string& body() const { return body_; }
    uint32_t hash() { return hash_; }

private:
    void Execute(RequestContext* ctx);

private:
    std::string uri_;
    std::string body_;
    uint32_t    hash_;
    HttpType    type_;
    std::map<std::string, std::string> headers_;
};

class GetRequest : public Request
{
public:
    GetRequest(const std::string& uri, const std::string& hash = "");
};

class PostRequest : public Request 
{
public:
    PostRequest(const std::string& uri, const std::string& body, 
        const std::string& hash = "");
};

} // namespace downstream
} // namespace shs
