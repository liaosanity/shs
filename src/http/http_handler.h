#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <boost/shared_ptr.hpp>

#include "http/http.h"

#include "types.h"

namespace shs 
{

class Framework;
class StatusHandlerContext;
class HttpInvokeParams;

void HttpReqHandler(HTTP_CODE ec, http_req_t *req, void *);
void HttpStatusHandler(HTTP_CODE ec, http_req_t *req, void *);
void HttpStatsHandler(HTTP_CODE ec, http_req_t *req, void *);

class SHSHttpHandler 
{
public:
    SHSHttpHandler(Framework* framework, http_req_t *req);
    ~SHSHttpHandler();

    void Invoke(InvokeCompleteHandler invoke_complete_handler,
        const std::string& module_name, const std::string& method_name,
        const std::map<std::string, std::string> &request_params,
        const int32_t timeout_ms,
        boost::shared_ptr<HttpInvokeParams> invoke_params);

    void InvokeReply(const InvokeResult& result);
    void StatusReply(const InvokeResult& result, 
        boost::shared_ptr<StatusHandlerContext> context);

public:
    std::string module_name_;
    std::string method_name_;
    std::map<std::string, std::string> params_;
    int32_t timeout_ms_;

private:
    Framework *framework_;
    http_req_t *req_;
};

} // namespace shs

#endif // HTTP_HANDLER_H
