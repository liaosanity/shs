#include "mod_proxy_test.h"
#include <unistd.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <set>
#include <netdb.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <tr1/functional>
#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <google/protobuf/text_format.h>
#include "output.h" 
#include "comm/logging.h"
#include "comm/thread_key.h"
#include "comm/config_engine.h"

LOG_NAME("ProxyTest");

NAME_SPACE_BS

ProxyTestModule::ProxyTestModule()
{
}

ProxyTestModule::~ProxyTestModule()
{    
}

void ProxyTestModule::LogHandler(const char* category,
    int level, const char* msg)
{
    if (NULL == category)
    {
        return;
    }

    NLOG(category, level) << msg;
}

bool ProxyTestModule::InitInMaster(const std::string& conf)
{
    cfg_.reset(new shs_conf::Config);
    cfg_engines_.reset(new shs_conf::ConfigEngines);

    if (!cfg_->Init(conf)) 
    {
        cout << "cfg_->Init() err, conf = " << conf << endl;

        return false;
    }
    
    string lf = cfg_->Section("settings")->Option()->GetStr("logging_conf");
    LOG_INIT(lf);

    if (!cfg_engines_->Init(cfg_)) 
    {
        LOG(ERROR) << "cfg_engines_->Init() err";

        return false;
    }

    string cfg_output;
    cfg_->PrintToString(&cfg_output);
    LOG(INFO) << cfg_output;

    output.SetLevel(INFO);
    output.SetOutputFunction(ProxyTestModule::LogHandler);

    LOG(INFO) << "InitInMaster done...";

    return true;
}

bool ProxyTestModule::InitInWorker(int *thread_num)
{
    *thread_num = cfg_->section("settings")->GetInt("workers", 0, 24);

    if (0 != thread_key_create()) 
    {
        LOG(ERROR) << "thread key create failed!";

        return false;
    }
    
    Register("test", std::tr1::bind(&ProxyTestModule::Test, this, 
        std::tr1::placeholders::_1, std::tr1::placeholders::_2, 
        std::tr1::placeholders::_3));

    Server::Option option(cfg_engines_->engine("test")->timeout_conn(), 
        cfg_engines_->engine("test")->timeout_recv(), cfg_engines_->engine("test")->retry());
    option.max_retry_req_num = cfg_engines_->engine("test")->GetInt("req_retry", 0, 1);

    port_ = cfg_engines_->engine("test")->port();
    uri_ = cfg_engines_->engine("test")->GetStr("uri");
    ips_ = cfg_engines_->engine("test")->hosts();
	
    for (int i = 0; i < ips_.size(); ++i) 
    {
        vector<string> tmp_list;
        tmp_list.push_back(ips_[i]);

        Server* srv = NULL;
        srv = new Server(this, port_, option);
        srv->AddMasterServer(tmp_list);
        servers_.push_back(srv);

        LOG(INFO) << "Init downstream server[" << ips_[i] << ":" << port_ << "], uri = " << uri_;
    }

    LOG(INFO) << "InitInWorker done, workers = " << *thread_num;

    return true;
}

void ProxyTestModule::Test(const map<string, string>& params, 
    const InvokeCompleteHandler& cb,
    boost::shared_ptr<InvokeParams> invoke_params)
{
    ContextPtr ctx(new Context);
    ctx->uid_ = GetParam(params, "uid");
    ctx->sid_ = GetParam(params, "sid");
    ctx->complete_handler_ = cb;

    ctx->tm_enqueue_ = (invoke_params->get_enqueue_time() - 
        invoke_params->get_request_time()) / 1000;
    ctx->tm_inqueue_ = (invoke_params->get_dequeue_time() - 
        invoke_params->get_enqueue_time()) / 1000;
    ctx->client_ip_ = invoke_params->get_client_ip();

    RequestUri uri(uri_);
    map<string, string>::const_iterator iter = params.begin();
    for (; iter != params.end(); ++iter) 
    {
        if ("postdata" == iter->first) 
        {
            continue;
        }

        uri.AddQuery(iter->first, iter->second);
    }

    ctx->timers_.Timer("all")->Start();

    srand(time(NULL));
    int len = rand() % 102400;
    if (len <=0) 
    {
        len = 100;
    }
    char *buf = (char *)malloc(len);
    string postdata = string(buf, len);
    free(buf);

    //string postdata = GetParam(params, "postdata");
    ctx->body_sz_ = postdata.size();
    if (ctx->body_sz_ > 0) 
    {
        ctx->request_.reset(new PostRequest(uri.uri(), postdata));
    } 
    else 
    {
        ctx->request_.reset(new GetRequest(uri.uri()));
    }

    for (int i = 0; i < servers_.size(); i++) 
    {
        stringstream timerkey;
        timerkey << "test_" << i;
        ctx->timers_.Timer(timerkey.str())->Start();
        ctx->req_num_++;

        ctx->request_->Execute(servers_[i], 
            std::tr1::bind(&ProxyTestModule::requestHandler, 
            this, std::tr1::placeholders::_1, std::tr1::placeholders::_2, 
            ctx, timerkey.str()));
    }
}

void ProxyTestModule::requestHandler(ErrCode ec, 
    boost::shared_ptr<Response> response, ContextPtr& ctx, string timerkey)
{
    ctx->timers_.Timer(timerkey)->Stop();
    ctx->test_timer_[timerkey] = ctx->timers_.Timer(timerkey)->Elapsed() * 1000;
    ctx->err_code_[timerkey] = ec;

    ctx->rsp_num_++;
    if (ctx->req_num_ != ctx->rsp_num_) 
    {
        return;
    }

    map<string, string> res;
    if (ec != downstream::OK || response->body().empty()) 
    {
        res["result"] = "downstream error";
        ctx->res_.assign("downstream error");
    } 
    else 
    {
        res["result"] = response->body();
        if (ctx->body_sz_ > 0 && ctx->body_sz_ != response->body().size()) 
        {
            ctx->res_.assign("response error");
        }
    }

    //if (0 == ctx->body_sz_) 
    //{
    //    ctx->res_ = response->body();
    //}

    InvokeResult iresult;
    iresult.set_results(res);
    ctx->complete_handler_(iresult);

    ctx->timers_.Timer("all")->Stop();

    ctx->OutputInfoLog();
}

string ProxyTestModule::GetParam(const map<string, string>& params, 
    const string& key, string default_value)
{
    map<string, string>::const_iterator iter;
    iter = params.find(key);
    if (iter == params.end()) 
    {
        return default_value;
    }

    return iter->second;
}

NAME_SPACE_ES
EXPORT_MODULE(srec::ProxyTestModule)

