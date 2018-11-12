#include "http_handler.h"

#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fstream>
#include <streambuf>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <gflags/gflags.h>

#include "log/logging.h"

#include "config.h"
#include "framework.h"
#include "http_invoke_params.h"
#include "process.h"
#include "stats.h"

namespace shs 
{

DECLARE_bool(rewrite_path_to_default);

using namespace std;
using namespace boost;

struct StatusHandlerContext 
{
    StatusHandlerContext() 
        : waiting_num(0) 
        , framework(NULL)
    {}
    size_t waiting_num;
    map<string, InvokeResult> modules_status;
    Framework* framework;
};

void HttpStatsHandler(HTTP_CODE ec, http_req_t *req, void *data)
{
    Framework* framework = (Framework *)data;
    Config* config = framework->config();
    string str_stats;

    str_stats += "<?xml version=\"1.0\" encoding=\"UTF-8\"?><stats>";
    str_stats += boost::str(boost::format("<stats_time>%1%</stats_time>") 
        % Timestamp::LocalTimeToString(Timestamp::Now().SecondsSinceEpoch()));
    str_stats += boost::str(boost::format("<exe>%1%</exe>") % get_exe_path());
    str_stats += boost::str(boost::format("<build>%1% %2%</build>") 
        % __DATE__ % __TIME__);
    str_stats += boost::str(boost::format("<start_time>%1%</start_time>") 
        % Timestamp::LocalTimeToString(g_stats->started));
    str_stats += boost::str(boost::format("<reload>%1%</reload>") 
        % g_stats->num_reload);

    if (g_stats->num_reload > 0)
    {
        str_stats += boost::str(boost::format("<last_reload>%1%</last_reload>") 
            % Timestamp::LocalTimeToString(g_stats->last_reload_time));
    }

    str_stats += boost::str(boost::format("<coredump>%1%</coredump>") 
        % g_stats->num_coredump);

    if (g_stats->num_coredump > 0)
    {
        str_stats += boost::str(boost::format("<last_coredump>%1%</last_coredump>") 
            % Timestamp::LocalTimeToString(g_stats->last_coredump_time));
    }

    str_stats += 
        boost::str(
            boost::format(
                "<config>"
                    "<num_processes>%1%</num_processes>"
                    "<timeout>%2%</timeout>"
                    "<queue_hwm>%3%</queue_hwm>"
                    "<queue_lwm>%4%</queue_lwm>"
                    "<status_file>%5%</status_file>"
                    "<http_port>%6%</http_port>"
                    "<module_path>%7%</module_path>"
                    "<module_conf>%8%</module_conf>"
                    "<pid_file>%9%</pid_file>"
                    "<log_file>%10%</log_file>"
                "</config>") 
            % config->num_processes() 
            % config->timeout() 
            % config->queue_hwm() 
            % config->queue_lwm() 
            % config->status_file() 
            % config->http_port() 
            % config->mod_path() 
            % config->mod_conf() 
            % config->pid_file() 
            % config->log_conf());

    if (g_stats->num_status != 0)
    {
        str_stats += "<status>";
        str_stats += boost::str(boost::format("<num_status>%1%</num_status>") 
            % g_stats->num_status);
        str_stats += boost::str(boost::format("<last_status>%1%</last_status>") 
            % (g_stats->last_status ? "OK" : "ERROR"));
        str_stats += boost::str(boost::format("<last_status_time>%1%</last_status_time>") 
            % Timestamp::LocalTimeToString(g_stats->last_status_time));

        if (g_stats->num_status_errors != 0)
        {
            str_stats += boost::str(boost::format("<num_status_errors>%1%</num_status_errors>") 
                % g_stats->num_status_errors);
            str_stats += boost::str(boost::format("<last_status_error_time>%1%</last_status_error_time>") 
                % Timestamp::LocalTimeToString(g_stats->last_status_error_time));
        }

        str_stats += "</status>";
    }

    str_stats += boost::str(boost::format("<queues ppid=\"%1%\">") % getppid());
    int64_t total_queue_size = 0;
    int64_t total_num_requests = 0;
    int64_t total_num_error_requests = 0;
    int64_t total_num_timeout_requests = 0;
    double  total_invoke_elapsed_time = 0.0;

    int64_t total_persistent_num_requests = 0;
    int64_t total_persistent_num_timeout_requests = 0;
    int64_t total_persistent_num_error_requests = 0;
    double  total_persistent_invoke_elapsed_time = 0.0;

    int index = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i)
    {
        if (g_processes[i].type == 1)
        {
            total_persistent_num_requests += g_stats->process_stats[i].persistent_num_requests;
            total_persistent_num_error_requests += g_stats->process_stats[i].persistent_num_error_requests;
            total_persistent_num_timeout_requests += g_stats->process_stats[i].persistent_num_timeout_requests;
            total_persistent_invoke_elapsed_time += g_stats->process_stats[i].persistent_invoke_elapsed_time;
        }
        
        if (g_processes[i].pid <= 0 || g_processes[i].type != 1)
        {
            continue;
        }

        total_queue_size += g_stats->process_stats[i].curr_queue_size;
        total_num_requests += g_stats->process_stats[i].num_requests;
        total_num_error_requests += g_stats->process_stats[i].num_error_requests;
        total_num_timeout_requests += g_stats->process_stats[i].num_timeout_requests;
        total_invoke_elapsed_time += g_stats->process_stats[i].invoke_elapsed_time;
        str_stats += 
            boost::str(
                boost::format("<queue id=\"%1%\" pid=\"%2%\" size=\"%3%\" max=\"%4%\" requests=\"%5%\" timeout=\"%6%\" error=\"%7%\" time-per-request=\"%8%\" queue-time-per-request=\"%9%\" />") 
                % index++
                % g_processes[i].pid
                % g_stats->process_stats[i].curr_queue_size 
                % g_stats->process_stats[i].max_queue_size
                % g_stats->process_stats[i].num_requests
                % g_stats->process_stats[i].num_timeout_requests
                % g_stats->process_stats[i].num_error_requests
                % g_stats->process_stats[i].persistent_avg_invoke_elapsed_time
                % g_stats->process_stats[i].persistent_avg_elapsed_time_in_queue);
    }

    g_stats->total_persistent_num_requests = total_persistent_num_requests;
    g_stats->total_persistent_num_error_requests = total_persistent_num_error_requests;
    g_stats->total_persistent_num_timeout_requests = total_persistent_num_timeout_requests;
    g_stats->total_persistent_invoke_elapsed_time = total_persistent_invoke_elapsed_time;

    if (g_stats->last_num_requests == 0 || total_num_requests == 0 
        || (total_num_requests - g_stats->last_num_requests) < 0)
    {
        g_stats->last_stats_time = Timestamp::Now();
        g_stats->last_num_requests = total_num_requests;
        g_stats->last_invoke_elapsed_time = total_invoke_elapsed_time;
        g_stats->time_per_request = 0.0;
        g_stats->qps = 0.0;
    }
    else
    {
        auto delta = Timestamp::Now().SecondsSinceEpoch() - g_stats->last_stats_time.SecondsSinceEpoch();
        if (delta >= 1)
        {
            g_stats->qps = (total_num_requests - g_stats->last_num_requests) / delta;
            g_stats->avg_qps = g_stats->total_persistent_num_requests / (Timestamp::Now().SecondsSinceEpoch() - g_stats->started);

            if (total_num_requests != g_stats->last_num_requests)
            {
                g_stats->time_per_request = (total_invoke_elapsed_time - g_stats->last_invoke_elapsed_time) / (total_num_requests - g_stats->last_num_requests);
            }
            else
            {
                g_stats->time_per_request = 0.0;
            }

            g_stats->last_stats_time = Timestamp::Now();
            g_stats->last_num_requests = total_num_requests;
            g_stats->last_invoke_elapsed_time = total_invoke_elapsed_time;
        }
    }

    str_stats += 
        boost::str(
            boost::format(
                "<total"
                    " num_processes=\"%ld\""
                    " queue_size=\"%ld\""
                    " requests=\"%ld\""
                    " timeout=\"%ld\""
                    " error=\"%ld\""
                    " time-per-request=\"%.6f\""
                    " requests-per-second=\"%.0f\""
                    " util=\"%.2f\""
                 " /></queues>") 
            % framework->config()->num_processes() 
            % total_queue_size 
            % total_num_requests 
            % total_num_timeout_requests 
            % total_num_error_requests
            % g_stats->time_per_request
            % g_stats->qps
            % (g_stats->qps*g_stats->time_per_request/framework->config()->num_processes())
            );

    str_stats += boost::str(
            boost::format(
                "<persistent" 
                    " requests=\"%ld\""
                    " timeout=\"%ld\""
                    " error=\"%ld\""
                    " time-per-request=\"%.6f\""
                    " queue-time-per-request=\"%.6f\""
                    " requests-per-second=\"%.0f\""
                " />")
            % g_stats->total_persistent_num_requests
            % g_stats->total_persistent_num_timeout_requests
            % g_stats->total_persistent_num_error_requests
            % Stats::GetAvgTimePerRequest()
            % Stats::GetAvgTimeInQueue()
            % g_stats->avg_qps
            );

    str_stats += "</stats>";

    http_add_output_header(req, "Content-Type", "text/xml; charset=UTF-8"); 
    http_send_reply(req, HTTP_OK, "OK", str_stats);
}

void HttpStatusHandler(HTTP_CODE ec, http_req_t *req, void *data)
{
    Framework* framework = (Framework *)data;

    map<string, bool>::iterator iter;
    boost::shared_ptr<StatusHandlerContext> context(new StatusHandlerContext);
    context->framework = framework;
    
    if (framework->config()->status_file() != "")
    {
        if (access(framework->config()->status_file().c_str(), F_OK) != 0)
        {
            goto failed;
        }
    }

    for (auto& module : framework->GetModulesAndMethods())
    {
        context->waiting_num++;

        boost::shared_ptr<SHSHttpHandler> handler(
            new SHSHttpHandler(framework, req));
        handler->module_name_ = module.first;
        handler->method_name_ = "Status";
        handler->timeout_ms_ = framework->config()->timeout();

        boost::shared_ptr<HttpInvokeParams> invoke_params(
            new HttpInvokeParams);

        invoke_params->set_request_time(
            Timestamp::Now().MicroSecondsSinceEpoch());
        invoke_params->set_protocol(req->major, req->minor);
        invoke_params->set_type(req->type);
        invoke_params->set_client_ip(req->hc->host);
        invoke_params->set_client_port(req->hc->port);
        for (int i = 0; i < HEADER_NUM; i++)
        {
            if (0 != req->input_headers[i].key.len 
                && 0 != req->input_headers[i].value.len)
            {
                invoke_params->set_header(
                    (const char *)req->input_headers[i].key.data, 
                    (const char *)req->input_headers[i].value.data);
            }
        }
        invoke_params->set_uri((const char *)req->uri.data);

        handler->Invoke(
            boost::bind(&SHSHttpHandler::StatusReply, handler, _1, context), 
            handler->module_name_, handler->method_name_, handler->params_, 
            handler->timeout_ms_, invoke_params);
    }

    return;

failed:
    http_send_reply(req, HTTP_SERVUNAVAIL, 
        get_reason_phrase(HTTP_SERVUNAVAIL), "");
}

void HttpReqHandler(HTTP_CODE ec, http_req_t *req, void *data)
{
    Framework* framework = (Framework *)data;
    Config* config = framework->config();
    boost::shared_ptr<HttpInvokeParams> invoke_params(
        new HttpInvokeParams);
    boost::shared_ptr<SHSHttpHandler> handler(
        new SHSHttpHandler(framework, req));
    const char* pstart = NULL;
    const char* pend = NULL;
    const char* req_uri = NULL;
    vector<string> m_name; 
    HttpQuery query;

    invoke_params->set_request_time(
        Timestamp::Now().MicroSecondsSinceEpoch());

    req_uri = (const char *)req->uri.data;
    if (NULL == req_uri)
    {
        goto failed;
    }

    pstart = req_uri;
    if (*pstart != '/')
    {
        goto failed;
    }

    pstart += 1;
    if ((pend = strchr(pstart, '/')) != NULL)
    {
        m_name.push_back(string(pstart, pend - pstart));
        pstart = pend + 1;
    }

    pend = strchr(pstart, '?');
    if (pend != NULL)
    {
        if (*(pend - 1) == '/')
        {
            pend -= 1;
        }

        if (pend > pstart)
        {
            m_name.push_back(string(pstart, pend - pstart));
        }
    }
    else
    {
        pend = req_uri + strlen(req_uri);
        string name(pstart, pend - pstart);
        if (name != "")
        {
            m_name.push_back(name);
        }
    }

    if (m_name.empty())
    {
        handler->module_name_ = config->default_module();
        handler->method_name_ = config->default_method();
    }
    else if (m_name.size() == 1)
    {
        handler->module_name_ = config->default_module();
        handler->method_name_ = m_name[0];
    }
    else if (m_name.size() == 2)
    {
        if (FLAGS_rewrite_path_to_default)
        {
            handler->module_name_ = config->default_module();
        }
        else
        {
            handler->module_name_ = m_name[0];
        }
        handler->method_name_ = m_name[1];
    }
    else
    {
        goto failed;
    }

    http_parse_query(req_uri, &query);
    for (HttpQuery::iterator it = query.begin(); it != query.end(); ++it)
    {
        handler->params_[it->first] = it->second;

        if (0 == strcasecmp(it->first.c_str(), "t"))
        {
            handler->timeout_ms_ = atoi(it->second.c_str());
        }
    }

    if (SHS_HTTP_REQ_TYPE_POST == req->type)
    {
        handler->params_["postdata"] = 
            std::string((const char *)req->input_body.data, 
            req->input_body.len);
    }

    if (handler->timeout_ms_ <= 0)
    {
        handler->timeout_ms_ = framework->config()->timeout(); 
    }

    for (int i = 0; i < HEADER_NUM; i++)
    {
        if (0 != req->input_headers[i].key.len 
            && 0 != req->input_headers[i].value.len)
        {
            invoke_params->set_header(
                (const char *)req->input_headers[i].key.data, 
                (const char *)req->input_headers[i].value.data);
        }
    }

    invoke_params->set_protocol(req->major, req->minor);
    invoke_params->set_type(req->type);
    invoke_params->set_client_ip(req->hc->host);
    invoke_params->set_client_port(req->hc->port);
    invoke_params->set_uri((const char *)req->uri.data);

    handler->Invoke(boost::bind(&SHSHttpHandler::InvokeReply, handler, _1), 
        handler->module_name_, handler->method_name_, handler->params_, 
        handler->timeout_ms_, invoke_params);

    return;

failed:
    http_send_reply(req, HTTP_BADREQUEST, 
        get_reason_phrase(HTTP_BADREQUEST), "");
}

SHSHttpHandler::SHSHttpHandler(Framework *framework, http_req_t *req)
    : timeout_ms_(0)
    , framework_(framework)
    , req_(req)
{
}

SHSHttpHandler::~SHSHttpHandler()
{
}

void SHSHttpHandler::StatusReply(const InvokeResult& result, 
    boost::shared_ptr<StatusHandlerContext> context)
{
    string response;

    context->modules_status[this->module_name_] = result;

    if (context->waiting_num != context->modules_status.size())
    {
        return;
    }

    g_stats->num_status++;
    g_stats->last_status_time = time(NULL);

    map<string, InvokeResult>::iterator iter;
    for (iter = context->modules_status.begin(); 
         iter != context->modules_status.end();
         ++iter)
    {
        string str_status;
        str_status = (*iter).first;
        if ((*iter).second.ec != ErrorCode::OK)
        {
            SLOG(ERROR) << "CheckStatus Module: " << iter->first 
                << " [ERROR] " << iter->second.msg;

            goto failed;
        }
    }

    try
    {
        ifstream ifs(context->framework->config()->status_file().c_str());
        response.assign((istreambuf_iterator<char>(ifs)), 
            (istreambuf_iterator<char>()));
    }
    catch(...)
    {
        SLOG(ERROR) << "HttpStatusHandler get '" 
            << context->framework->config()->status_file() << " failed.";

        goto failed;
    }

    if (response == "")
    {
        response = "ok";
    }

    http_send_reply(req_, HTTP_OK, "OK", response);
    g_stats->last_status = true;

    return;

failed:
    http_send_reply(req_, HTTP_SERVUNAVAIL, 
        get_reason_phrase(HTTP_SERVUNAVAIL), "");

    g_stats->last_status = false;
    g_stats->last_status_error_time = time(NULL);
    g_stats->num_status_errors++;
}

void SHSHttpHandler::InvokeReply(const InvokeResult& result)
{
    int response_code = 404;
    std::string reason_phrase;
    std::string data;

    if (ErrorCode::OK == result.ec)
    {   
        auto it = result.results.find("result");
        if (it != result.results.end())
        {
            data = it->second;
        }

        bool user_define_response_code = false;
        for (auto& kv : result.results)
        {
            if (kv.first == "result")
            {
                continue;
            }

            if (kv.first == "response_code")
            {
                user_define_response_code = true;
                response_code = atoi(kv.second.c_str());

                continue;
            }

            if (kv.first == "reason_phrase")
            {
                reason_phrase = kv.second;

                continue;
            }

            if (0 == strcasecmp(kv.first.c_str(), "Set-Cookie") 
                && kv.second.length() > 0)
            {
                uint16_t count = 
                    *(reinterpret_cast<const uint16_t*>(kv.second.data()));
                const char* curr = kv.second.c_str() + 2;
                for (uint16_t i = 0;i < count;i++)
                {
                    uint16_t length = 
                        *(reinterpret_cast<const uint16_t*>(curr));
                    curr += 2;

                    http_add_output_header(req_, kv.first, 
                        std::string(curr, length));
                    curr += length;
                }
            }
            else
            {
                http_add_output_header(req_, kv.first, kv.second);
            }
        }

        if (!user_define_response_code && data.empty())
        {
            SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
                << "\tBad response, result is empty";
        }
        else
        {
            if (result.results.find("Content-Type") == result.results.end())
            {
                http_add_output_header(req_, 
                    "Content-Type", "text/plain; charset=UTF-8");
            }
        
            if (user_define_response_code)
            {
                if (reason_phrase.empty())
                {
                    reason_phrase = get_reason_phrase(response_code);

                    if (reason_phrase.empty())
                    {
                        response_code = 404;
                    }
                }
            }
            else
            {
                response_code = 200;
            }
        }
    }   
    else
    {   
        SLOG(ERROR) << __FILE__ << ":" << __LINE__ 
            << "\tBad response\tec=" << result.ec << "\tmsg=" << result.msg; 
    }

    if (reason_phrase.empty())
    {
        reason_phrase = get_reason_phrase(response_code);
    }

    http_send_reply(req_, response_code, reason_phrase, data);
}

void SHSHttpHandler::Invoke(InvokeCompleteHandler complete_handler,
    const string& module_name, const string& method_name,
    const map<string, string> & request_params, const int32_t timeout_ms,
    boost::shared_ptr<HttpInvokeParams> invoke_params) 
{
    framework_->Invoke(module_name, method_name, request_params,
        timeout_ms, complete_handler, invoke_params);
}

} // namespace shs
