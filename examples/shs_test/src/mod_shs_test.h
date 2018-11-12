#ifndef MOD_SHS_TEST_H
#define MOD_SHS_TEST_H

#include <boost/thread.hpp>
#include "common.h"
#include "module.h"
#include "http_invoke_params.h"
#include "comm/config_engine.h"

using namespace shs;
using namespace std;
using namespace boost;

NAME_SPACE_BS

class ShsTestModule : public shs::Module
{
public:
    ShsTestModule();
    ~ShsTestModule();

    static void LogHandler(const char* category, int level, const char* msg);

    bool InitInMaster(const std::string& conf);
    bool InitInWorker(int *thread_num);
    
    void Test(const std::map<std::string, std::string>& params, 
        const shs::InvokeCompleteHandler& cb,
        boost::shared_ptr<InvokeParams> invoke_params);

private:
    std::string GetParam(const std::map<std::string, std::string>& params,
        const std::string& key, std::string default_value = "");

private:
    boost::shared_ptr<shs_conf::Config> cfg_;
    boost::shared_ptr<shs_conf::ConfigEngines> cfg_engines_;

};

NAME_SPACE_ES

#endif

