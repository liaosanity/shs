#include "server.h"

#include <algorithm>

#include "downstream/host.h"

#include "module.h"

namespace shs 
{
namespace downstream 
{

Server::Server(Module* module, int port, const Option& option)
    : module_(module)
    , port_(port)
    , option_(option)
    , group_(HostGroupProvider::instance()->Create(this))
{
}

Server::~Server()
{
    HostGroupProvider::instance()->Release(group_);
}

void Server::AddMasterServer(const std::vector<std::string>& hosts)
{
    group_->AddMasterServer(hosts, port_);
}

void Server::AddSlaveServer(const std::vector<std::string>& hosts)
{
    group_->AddSlaveServer(hosts, port_);
}

Host* Server::GetMasterHost(size_t hash)
{
    return group_->GetMasterHost(hash);
}

Host* Server::GetMasterHostByIndex(size_t index)
{
    return group_->GetMasterHostByIndex(index);
}

Host* Server::Create(size_t hash, int retry_cnt,
    std::set<int>& history_hosts, ErrCode* ec)
{
    Host* host = group_->SelectHost(hash, history_hosts, retry_cnt);
    if (!host) 
    {   
        *ec = E_SELECT_HOST_FAIL;

        return NULL;
    }

    return host;
}

size_t Server::GetMasterHostSize() const
{
    return group_->GetMasterHostSize();
}

const std::string Server::type() const
{
    return "Http";
}

event_base_t* Server::event_base() const
{
    return module_ ? module_->event_base() : NULL;
}

event_timer_t* Server::event_timer() const
{
    return module_ ? module_->event_timer() : NULL;
}

conn_pool_t* Server::conn_pool() const
{
    return module_ ? module_->conn_pool() : NULL;
}

std::string Server::name() const
{
    if (name_.empty())
    {
        return "server_" + std::to_string(port_) + "_" 
            + std::to_string(group_->data()->index);
    }

    return name_;
}

} // namespace downstream
} // namespace shs
