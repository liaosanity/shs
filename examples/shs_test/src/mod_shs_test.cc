#include "mod_shs_test.h"
#include <sstream>
#include "output.h" 
#include "comm/logging.h"
#include "comm/thread_key.h"

LOG_NAME("ShsTest");

NAME_SPACE_BS

ShsTestModule::ShsTestModule()
{
}

ShsTestModule::~ShsTestModule()
{    
}

void ShsTestModule::LogHandler(const char* category,
    int level, const char* msg)
{
    if (NULL == category)
    {
        return;
    }

    NLOG(category, level) << msg;
}

bool ShsTestModule::InitInMaster(const std::string& conf)
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
    output.SetOutputFunction(ShsTestModule::LogHandler);

    LOG(INFO) << "InitInMaster done...";

    return true;
}

bool ShsTestModule::InitInWorker(int *thread_num)
{
    *thread_num = cfg_->section("settings")->GetInt("workers", 0, 24);

    if (0 != thread_key_create()) 
    {
        LOG(ERROR) << "thread key create failed!";

        return false;
    }
    
    Register("test", std::tr1::bind(&ShsTestModule::Test, this, 
        std::tr1::placeholders::_1, std::tr1::placeholders::_2, 
        std::tr1::placeholders::_3));

    LOG(INFO) << "InitInWorker done, workers = " << *thread_num;

    return true;
}

void ShsTestModule::Test(const map<string, string>& params, 
    const InvokeCompleteHandler& cb,
    boost::shared_ptr<InvokeParams> invoke_params)
{
    string uid = GetParam(params, "uid");
    string sid = GetParam(params, "sid");
    string postdata = GetParam(params, "postdata");

    stringstream msg;
    msg << "Hello uid = " << uid << ", sid = " << sid 
        << ", body_sz = " << postdata.size() << ", I'm test, are you ok!!!";

    LOG(INFO) << msg.str();

    map<string, string> res;
    res["result"] = (postdata.empty()) ? msg.str() : postdata;
    InvokeResult iresult;
    iresult.set_results(res);
    cb(iresult);
}

string ShsTestModule::GetParam(const map<string, string>& params, 
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
EXPORT_MODULE(srec::ShsTestModule)

