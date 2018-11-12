#ifndef CONFIG_ENGINE_H
#define CONFIG_ENGINE_H

#include <string>
#include <stdint.h>
#include <map>
#include <vector>
#include <iostream>
#include <boost/format.hpp>
#include <tr1/functional>

#include "config.h"

namespace shs_conf 
{

class ConfigUri
{
public:
    ConfigUri();
    ~ConfigUri();

    bool Init(const cJSON* jnode);
    std::map<std::string, std::string > Make(const std::map<std::string, 
        std::string>& req_params) const;

private:
    std::map<std::string, std::map<std::string, std::string> > params_;
};

class ConfigEngine : public ConfigOption
{
public:
    ConfigEngine();
    virtual ~ConfigEngine();

    bool Init(const std::string& name, 
        boost::shared_ptr<shs_conf::Config> config);
    bool IsValid() const;
    bool SendAllow(const std::string& hash, const std::string& ip) const;
    void Print() const;
    void PrintToString(std::string* output) const;

    int32_t thread_num() const { return thread_num_; }
    int32_t timeout_conn() const { return timeout_conn_; }
    int32_t timeout_recv() const { return timeout_recv_; }
    int32_t timeout_send() const { return timeout_send_; }
    int32_t retry() const { return retry_; }
    int32_t port() const { return port_; }
    bool enabled() const { return enabled_; }
    int32_t flow_rate() const { return flow_rate_; }
    const std::string& host() const { return hosts_.front(); }
    const std::vector<std::string>& hosts() const { return hosts_; }
    const std::string& name() const { return name_; }

    ConfigUri& uri() { return uri_; }

protected:
    int32_t port_;
    int32_t timeout_conn_;
    int32_t timeout_recv_;
    int32_t timeout_send_;
    int32_t retry_;
    bool    enabled_;
    int32_t flow_rate_;
    int32_t thread_num_;
    std::vector<std::string> hosts_;
    std::string name_;
    ConfigUri   uri_;
    boost::shared_ptr<shs_conf::Config> config_;
};

class ConfigEngines
{
public:
    ConfigEngines();
    ~ConfigEngines();

    bool Init(boost::shared_ptr<shs_conf::Config> config);
    boost::shared_ptr<ConfigEngine> engine(const std::string& name) const;
    void Print() const;
    void PrintToString(std::string* output) const;

private:
    std::map<std::string, boost::shared_ptr<ConfigEngine> > engines_;
};

} // namespace shs_conf

#endif
