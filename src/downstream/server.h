#pragma once

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>

#include "downstream/err_code.h"
#include "downstream/host.h"
#include "core/shs_epoll.h"
#include "core/shs_conn_pool.h"

namespace shs 
{

class Module;

namespace downstream 
{

class Host;
class HostGroup;

class Server : boost::noncopyable 
{
public:
    struct Option
    {
        uint32_t timeout_con;
        uint32_t timeout_rcv;
        uint32_t max_retry_con_num;
        uint32_t max_retry_req_num;

        Option(uint32_t timeout_conn, uint32_t timeout_recv, uint32_t retry)
            : timeout_con(timeout_conn)
            , timeout_rcv(timeout_recv)
            , max_retry_con_num(retry)
            , max_retry_req_num(0)
        {}
    };

    Server(Module* module, int port, const Option& Option);
    virtual ~Server();

    Host* Create(size_t hash, int retry_cnt,
        std::set<int> &history_hosts, ErrCode* ec);

    void AddMasterServer(const std::vector<std::string>& hosts);
    void AddSlaveServer(const std::vector<std::string>& hosts);

    Module* module() const { return module_; }
    uint32_t max_retry_num() const { return option_.max_retry_req_num; }
    uint32_t max_timeout() const { return option_.timeout_con + option_.timeout_rcv; }
    const Option& option() const { return option_; }
    HostGroup* group() const { return group_; }
    size_t GetAliveHostCount() const { return group_->GetAliveHostCount(); }
    uint16_t port() const { return port_; } 
    void set_name(const std::string& name) { name_ = name; }

    const std::string type() const;

    event_base_t* event_base() const;
    event_timer_t* event_timer() const;
    conn_pool_t* conn_pool() const;

    size_t GetMasterHostSize() const;
    Host* GetMasterHost(size_t hash);
    Host* GetMasterHostByIndex(size_t index);

    std::string name() const;

private:
    Module* module_;
    uint16_t port_;
    Option option_;

    HostGroup* group_;
    std::string name_;
};

} // namespace downstream
} // namespace shs
