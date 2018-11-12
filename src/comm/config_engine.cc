#include <stdio.h>

#include <iostream>
#include <string>
#include <string.h>
#include <stdint.h>
#include <tr1/functional>
#include <iomanip>
#include <boost/format.hpp>

#include "config_engine.h"
#include "util.h"

namespace shs_conf 
{

using namespace std;
using namespace std::tr1::placeholders;

ConfigUri::ConfigUri()
{
}

ConfigUri::~ConfigUri()
{
}

bool ConfigUri::Init(const cJSON* jnode)
{
    if (NULL == jnode || jnode->type != cJSON_Array)
    {
        return false;
    }

    int size = cJSON_GetArraySize((cJSON*)jnode);
    for (int i = 0; i < size; ++i)
    {
        cJSON* jitem = NULL;
        cJSON* jname = NULL;
        cJSON* jvalue = NULL;
        cJSON* jencode = NULL;
        cJSON* jalias = NULL;
        cJSON* jkeep = NULL;
        bool encode = false;
        string str_alias;
        string str_keep;

        jitem = cJSON_GetArrayItem((cJSON*)jnode, i);
        if (NULL == jitem)
        {
            continue;
        }

        jname = cJSON_GetObjectItem(jitem, "name");
        if (jname == NULL || jname->type != cJSON_String 
            || jname->valuestring == NULL || strlen(jname->valuestring) == 0)
        {
            continue;
        }

        jvalue = cJSON_GetObjectItem(jitem, "value");
        if (jvalue == NULL || jvalue->type != cJSON_String 
            || jvalue->valuestring == NULL)
        {
            continue;
        }

        jkeep = cJSON_GetObjectItem(jitem, "keep");
        if (jkeep != NULL && jkeep->type == cJSON_String 
            && jkeep->valuestring != NULL && strlen(jkeep->valuestring) != 0)
        {
            str_keep = jkeep->valuestring;
        }

        jencode = cJSON_GetObjectItem(jitem, "encode");
        if (jencode != NULL && (jencode->type == cJSON_False 
            || jencode->type == cJSON_True))
        {
            encode = jencode->valueint;
        }

        jalias = cJSON_GetObjectItem(jitem, "alias");
        if (jalias != NULL && jalias->type == cJSON_String 
            && jalias->valuestring != NULL && strlen(jalias->valuestring) != 0)
        {
            str_alias = jalias->valuestring;
        }
        else
        {
            str_alias = jname->valuestring;
        }

        params_[jname->valuestring]["value"] = jvalue->valuestring;
        params_[jname->valuestring]["alias"] = str_alias;
        params_[jname->valuestring]["encode"] = encode ? "1" : "0";
        params_[jname->valuestring]["keep"] = str_keep;
    }

    return true;
}

map<string, string> ConfigUri::Make(const map<string, string>& req_params) const
{
    map<string, string>::const_iterator iter_req_params = req_params.begin();
    map<string, map<string, string> >::const_iterator iter_cfg_params = params_.begin();
    map<string, string>  out;

    for (; iter_cfg_params != params_.end(); ++iter_cfg_params)
    {
        string name = iter_cfg_params->first;
        string value;
        string alias;
        string encode;
        string keep;
        map<string, string>::const_iterator iter_param_value;
        iter_param_value = iter_cfg_params->second.find("value");
        if (iter_param_value != iter_cfg_params->second.end())
        {
            value = iter_param_value->second;
        }

        iter_param_value = iter_cfg_params->second.find("alias");
        if (iter_param_value != iter_cfg_params->second.end())
        {
            alias = iter_param_value->second;
        }

        iter_param_value = iter_cfg_params->second.find("encode");
        if (iter_param_value != iter_cfg_params->second.end())
        {
            encode = iter_param_value->second;
        }

        iter_param_value = iter_cfg_params->second.find("keep");
        if (iter_param_value != iter_cfg_params->second.end())
        {
            keep = iter_param_value->second;
        }

        if (keep != "1")
        {
            iter_req_params = req_params.find(alias);
            if (iter_req_params != req_params.end())
            {
                value = iter_req_params->second;
            }
        }

        out[name] = value;
    }

    return out;
}

ConfigEngine::ConfigEngine()
    : port_(0)
    , timeout_conn_(0)
    , timeout_recv_(0)
    , timeout_send_(0)
    , retry_(0)
    , enabled_(false)
    , flow_rate_(100)
    , thread_num_(1)
{
}

ConfigEngine::~ConfigEngine()
{
}

bool ConfigEngine::Init(const string& name, 
    boost::shared_ptr<shs_conf::Config> config)
{
    if (!config->HasSection(name))
    {
        return false;
    }

    if (config->Section(name)->OptionSize() != 1)
    {
        return false;
    }

    boost::shared_ptr<ConfigOption> option = config->Section(name)->Option();

    port_ = option->GetInt("port", 0, 0);
    retry_ = option->GetInt("retry", 0);
    enabled_ = option->GetInt("enabled", 0);
    flow_rate_ = option->GetInt("flow_rate", 0, 100);
    thread_num_ = option->GetInt("thread_num", 0, 1);
    timeout_conn_ = option->GetInt("timeout_conn", 0);
    timeout_recv_ = option->GetInt("timeout_recv", 0);
    timeout_send_ = option->GetInt("timeout_send", 0);

    for (size_t i = 0; i < option->GetSize("host"); ++i)
    {
        string str_host = option->GetStr("host", i);
        if (str_host != "")
        {
            hosts_.push_back(str_host);
        }
    }

    name_ = name;
    option_ = option->option();
    config_ = config;

    return true;
}

bool ConfigEngine::IsValid() const
{
    if (!enabled_)
    {
        return false;
    }

    if (port_ <= 0 || hosts_.size() == 0 || thread_num_ <= 0)
    {
        return false;
    }

    return true;
}

bool ConfigEngine::SendAllow(const string& hash, const string& ip) const
{
    if (!IsValid())
    {
        return false;
    }

    size_t hash_num = tr1::hash<string>()(hash);
    int modulus = (hash_num % 100) + 1;
    if (modulus <= flow_rate_)
    {
        return true;
    }
    else
    {
        const char* address = ip.c_str();
        if (strncmp(address, "127", 3) == 0 ||
            strncmp(address, "10.", 3) == 0 ||
            strncmp(address, "192.168", 7) == 0 ||
            strcmp(address, "123.125.74.6") == 0 ||
            strcmp(address, "220.181.126.42") == 0 ||
            strcmp(address, "220.181.126.4") == 0 ||
            strcmp(address, "203.187.187.130") == 0)
        {
            return true;
        }

        for (int i = 16; i <= 31; i++)
        {
            char tmp[8];
            sprintf(tmp, "172.%d.", i);
            if (strncmp(address, tmp, strlen(tmp)) == 0)
            {
                return true;
            }
        }
    }

    return false;
}

void ConfigEngine::Print() const
{
    string str_hosts;
    for (size_t i = 0; i < hosts_.size(); ++i)
    {
        str_hosts += hosts_[i] + ";";
    }
    cout << setw(12) << name_ << "      enabled=[" << enabled_<< "]" << endl;
    cout << setw(12) << name_ << "         host=[" << str_hosts << "]" << endl;
    cout << setw(12) << name_ << "         port=[" << port_ << "]" << endl;
    cout << setw(12) << name_ << "        retry=[" << retry_ << "]" << endl;
    cout << setw(12) << name_ << " timeout_conn=[" << timeout_conn_ << "]" << endl;
    cout << setw(12) << name_ << " timeout_recv=[" << timeout_recv_ << "]" << endl;
    cout << setw(12) << name_ << " timeout_send=[" << timeout_send_ << "]" << endl;
    cout << setw(12) << name_ << "    flow_rate=[" << flow_rate_ << "]" << endl;
    cout << setw(12) << name_ << "   thread_num=[" << thread_num_ << "]" << endl;

    return;
}

ConfigEngines::ConfigEngines()
{
}

ConfigEngines::~ConfigEngines()
{
}

bool ConfigEngines::Init(boost::shared_ptr<shs_conf::Config> config)
{
    if (!config->HasSection("engines"))
    {
        return false;
    }

    if (config->Section("engines")->OptionSize() != 1)
    {
        return false;
    }

    boost::shared_ptr<ConfigOption> option = config->Section("engines")->Option();

    if (!option->HasOption("engine"))
    {
        return false;
    }

    size_t engine_count = option->GetSize("engine");
    for (size_t i = 0; i < engine_count; ++i)
    {
        string engine_name = option->GetStr("engine", i, "");
        if (engine_name == "")
        {
            continue;
        }

        boost::shared_ptr<ConfigEngine> engine(new ConfigEngine);
        if (!engine->Init(engine_name, config))
        {
            continue;
        }

        string path_params = boost::str(boost::format("/%1%/params") % engine_name);
        const cJSON* jparams = config->JsonObject(path_params);
        if (jparams != NULL)
        {
            engine->uri().Init(jparams);
        }

        engines_[engine_name] = engine;
    }

    return true;
}

boost::shared_ptr<ConfigEngine> ConfigEngines::engine(const std::string& name) const
{
    std::map<std::string, boost::shared_ptr<ConfigEngine> >::const_iterator iter;
    iter = engines_.find(name);
    if (iter == engines_.end())
    {
        return boost::shared_ptr<ConfigEngine>(new ConfigEngine);
    }

    return iter->second;
}

void ConfigEngines::Print() const
{
    std::map<std::string, boost::shared_ptr<ConfigEngine> >::const_iterator iter;
    for (iter = engines_.begin(); iter != engines_.end(); ++iter)
    {
        iter->second->Print();
    }
}

void ConfigEngines::PrintToString(string* output) const
{
}

} // namespace shs_conf
