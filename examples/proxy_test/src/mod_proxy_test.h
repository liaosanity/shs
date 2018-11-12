#ifndef MOD_PROXY_TEST_H
#define MOD_PROXY_TEST_H

#include <map>
#include <string>
#include <boost/thread.hpp>
#include "common.h"
#include "module.h"
#include "http_invoke_params.h"
#include "downstream/err_code.h"
#include "downstream/server.h"
#include "downstream/request.h"
#include "downstream/response.h"
#include "comm/config_engine.h"
#include "context.h"

using namespace std;
using namespace boost;
using namespace shs;
using namespace shs::downstream;

NAME_SPACE_BS

class ProxyTestModule : public shs::Module
{
public:
    ProxyTestModule();
    ~ProxyTestModule();

    static void LogHandler(const char* category, int level, const char* msg);
    
    bool InitInMaster(const std::string& conf);
    bool InitInWorker(int *thread_num);
    
    void Test(const std::map<std::string, std::string>& params, 
        const shs::InvokeCompleteHandler& cb,
        boost::shared_ptr<InvokeParams> invoke_params);

private:
    std::string GetParam(const std::map<std::string, std::string>& params,
        const std::string& key, std::string default_value = "");
    void requestHandler(ErrCode ec, boost::shared_ptr<Response> response, 
        ContextPtr& ctx, string timerkey);

private:
    boost::shared_ptr<shs_conf::Config> cfg_;
    boost::shared_ptr<shs_conf::ConfigEngines> cfg_engines_;

    vector<Server *> servers_;
    int port_;
    vector<string> ips_;
    string uri_;

};

NAME_SPACE_ES

#endif

