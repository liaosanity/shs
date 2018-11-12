#include "host.h"

#include <string.h>
#include <pthread.h>
#include <algorithm>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/anonymous_shared_memory.hpp>
#include <gflags/gflags.h>

#include "log/logging.h"
#include "comm/macros.h"
#include "downstream/util.h"
#include "downstream/server.h"

typedef boost::interprocess::scoped_lock<
    boost::interprocess::interprocess_mutex> ScopedLock;

extern uint32_t g_process;
extern uint32_t g_process_generation;

namespace shs 
{

DECLARE_int32(num_processes);

namespace downstream 
{

Host::Host()
    : port_(0)
    , backup_(false)
    , fail_num_(0)
    , status_fail_num_(0)
    , group_idx_(-1)
    , host_idx_(-1)
    , is_online_(true)
    , last_idx_(0)
{
    memset(ip_, 0, sizeof ip_);
    tag_[0] = '\0';
    bzero(&history_stats_, sizeof history_stats_);
}

void Host::Reset()
{
    port_ = 0;
    backup_ = false;
    fail_num_ = 0;
    status_fail_num_ = 0;
    last_time_ = Timestamp();
    down_start_ = Timestamp();

    group_idx_ = -1;
    host_idx_ = -1;

    memset(ip_, 0, sizeof ip_);
    tag_[0] = '\0';

    is_online_ = true;

    bzero(&history_stats_, sizeof history_stats_);
    last_idx_ = 0;
}

void Host::Init(const std::string& ip, uint16_t port, bool backup)
{
    strncpy(ip_, ip.c_str(), sizeof ip_);
    ip_[sizeof(ip_)-1] = '\0';
    port_ = port;
    backup_ = backup;
}

void Host::UpdateStats(Result result, Server* server)
{
    ScopedLock lock(mutex_);

    if (result == kFail)
    {
        ++fail_num_;
    }
    else if (result == kOk)
    {
        fail_num_ = 0;
        down_start_ = Timestamp();
    }
    else if (result == kStatusFail)
    {
        ++status_fail_num_;
        if (status_fail_num_ >= 3)
        {
            is_online_ = false;
        }
    }
}

bool Host::CheckAlive(Server* server, bool strict, bool do_skip)
{
    ScopedLock lock(mutex_);

    Timestamp now(Timestamp::Now());
    bool healthy = IsHealthy(server, strict, do_skip);
    if (!healthy)
    {
        SLOG(ERROR)<< "Host::CheckAlive: host(" << ip_ << ":" 
            << port_ << ") is not alive. params: "
            << strict << " " << do_skip << " "
            << is_online_ << ":" << status_fail_num_ << " "
            << last_time_.MicroSecondsSinceEpoch()/1000 << " "
            << now.MicroSecondsSinceEpoch()/1000 << " "
            << TimeDifference(now, last_time_)/1000;
        if (!down_start_.Valid())
        {
            down_start_ = now;
        }
    }
    else
    {
        if (do_skip)
        {
            last_time_ = now;
        }
    }

    return healthy;
}

bool Host::IsHealthy(Server* server, bool strict, bool do_skip)
{
    if (!is_online_)
    {
        return false;
    }

    if (0 == fail_num_)
    {
        return true;
    }

    if (!strict && fail_num_ < 3)
    {
        return true;
    }

    if (last_time_.Valid() && Timestamp::Now() > AddTime(last_time_, 0))
    {
        return true;
    }

    return false;
}

const std::string Host::State(Server* server) const
{
    if (!is_online_)
    {
        return "down";
    }

    if (fail_num_ < 3)
    {
        return "up";
    }

    if (down_start_.Valid())
    {
        return "down";
    }

    return "unhealthy";
}

std::string Host::ip_port() const
{
    return ip() + ":" + std::to_string(port_);
}

void Host::gen_tag()
{
    if (group_idx_ >= 0 && host_idx_ >= 0)
    {
        snprintf(tag_, sizeof tag_, "host_%d_%d_%s", 
            group_idx_, host_idx_, backup_ ? "s" : "m");
    }
    else
    {
        snprintf(tag_, sizeof tag_, "host_%s", ip_);
    }
}

void Host::SetOnline()
{
    ScopedLock lock(mutex_);
    is_online_ = true;
}

void Host::SetOffline()
{
    ScopedLock lock(mutex_);
    is_online_ = false;
}

bool Host::IsOnline() const
{
    ScopedLock lock(mutex_);
    return is_online_;
}

void HostGroupData::Reset()
{
    for (uint16_t i = 0;i < master_hosts_cnt;i++)
    {
        master_hosts[i].Reset();
    }

    for (uint16_t i = 0;i < slave_hosts_cnt; i++)
    {
        slave_hosts[i].Reset();
    }

    master_hosts_cnt = 0;
    slave_hosts_cnt = 0;

    group = NULL;
    index = -1;

    ref_cnt = 0;
    generation = 0;
}

Host* HostGroupData::AddMaster()
{
    if (master_hosts_cnt >= arraysize(master_hosts))
    {
        return NULL;
    }

    master_hosts[master_hosts_cnt].set_host_idx(master_hosts_cnt);

    return &master_hosts[master_hosts_cnt++];
}

Host* HostGroupData::AddSlave()
{
    if (slave_hosts_cnt >= arraysize(slave_hosts))
    {
        return NULL;
    }

    slave_hosts[slave_hosts_cnt].set_host_idx(slave_hosts_cnt);

    return &slave_hosts[slave_hosts_cnt++];
}

Host* HostGroupData::GetMaster(size_t index)
{
    if (index >= master_hosts_cnt)
    {
        return NULL;
    }

    return &master_hosts[index];
}

Host* HostGroupData::GetSlave(size_t index)
{
    if (index >= slave_hosts_cnt)
    {
        return NULL;
    }

    return &slave_hosts[index];
}

HostGroup::HostGroup(Server* server, HostGroupData* data)
    : data_(data),
    server_(server)
{
}

void HostGroup::AddMasterServer(const std::vector<std::string>& ips, 
    uint16_t port)
{
    for (auto& ip : ips)
    {
        Host* host = data_->AddMaster();
        if (!host)
        {
            break;
        }

        host->Init(ip, port, false);
        host->set_group_idx(data_->index);
        host->gen_tag();
        hosts_[ip] = host;
    }

    HashInit();
}

void HostGroup::AddSlaveServer(const std::string& ip, uint16_t port)
{
    Host* host = data_->AddSlave();
    if (!host)
    {
        return;
    }

    host->Init(ip, port, true);
    host->set_group_idx(data_->index);
    host->gen_tag();
    hosts_[ip] = host;
}

void HostGroup::AddSlaveServer(const std::vector<std::string>& ips, 
    uint16_t port)
{
    for (auto& ip : ips)
    {
        AddSlaveServer(ip, port);
    }
}

int HostGroup::GetPos(size_t hash)
{
    int pos = virtual_node_set_.size();
    for (unsigned int i = 0; i < virtual_node_set_.size(); i++)
    {
        VirtualNode& virtual_node = virtual_node_set_[i];
        if (hash < virtual_node.hash_offset)
        {
            pos = i;

            break;
        }
    }

    return pos;
}

void HostGroup::HashInit()
{
    virtual_node_set_.clear();

    for (unsigned int idx = 0; idx < data_->master_hosts_cnt; idx++)
    {
        auto& host = data_->master_hosts[idx];

        for (int i = 0; i < 200; i++)
        {
            std::string str(host.ip() + "_" + std::to_string(host.port()) 
                + "_" + std::to_string(i));
            auto hash = CalcHash(str.c_str(), str.size());
            VirtualNode virtual_node;
            virtual_node.idx = idx;
            virtual_node.hash_offset = hash;
            int pos = GetPos(hash);
            virtual_node_set_.insert(virtual_node_set_.begin()+pos, virtual_node);
        }
    }

    uint32_t avg = virtual_node_set_.back().hash_offset/virtual_node_set_.size();
    for (uint32_t l = 0; l < virtual_node_set_.size(); l++)
    {
        virtual_node_set_[l].hash_offset = avg * (l + 1);
    }
}

int HostGroup::GetHashIdx(int start, int end, size_t hash)
{
    int middle = (start+end) / 2;

    VirtualNode& virtual_node = virtual_node_set_[middle];

    if (hash == virtual_node.hash_offset)
    {
        return middle;
    }

    if (hash < virtual_node.hash_offset)
    {
        if ((middle == start) 
            || (hash > virtual_node_set_[middle - 1].hash_offset))
        {
            return middle;
        }
        
        return GetHashIdx(start, middle - 1, hash);

    }

    if ((middle == start) || (hash < virtual_node_set_[middle + 1].hash_offset))
    {
        return (middle + 1);
    }

    return GetHashIdx(middle + 1, end, hash);
}

Host* HostGroup::SelectHost(size_t hash, std::set<int>& history_hosts, 
    int retry_cnt)
{
    Host* host = Master(hash, history_hosts);
    if (!host)
    {
        return Slave(hash, retry_cnt);
    }

    return host;
}

Host* HostGroup::Master(size_t hash, std::set<int>& history_hosts)
{
    std::set<int> fail_hosts = history_hosts;
    if (virtual_node_set_.empty())
    {
        SLOG(ERROR) << "SelectHost fail, virtual_node_set_ is empty";

        return NULL;
    }

    int idx = GetHashIdx(0, virtual_node_set_.size(), hash) 
        % virtual_node_set_.size();
    if (idx < 0)
    {
        SLOG(ERROR) << "SelectHost fail, invalid virtual node idx";

        return NULL;
    }

    int host_idx = virtual_node_set_[idx].idx;
    Host* host = data_->GetMaster(host_idx);
    if (!host)
    {
        return NULL;
    }

    if (fail_hosts.find(host_idx) == fail_hosts.end()) 
    {
        history_hosts.insert(host_idx);

        return host;
    }

    fail_hosts.insert(host_idx);

    for (unsigned int i = 0; i < virtual_node_set_.size(); i++)
    {
        idx = (idx+1) % virtual_node_set_.size();
        host_idx = virtual_node_set_[idx].idx;
        if (fail_hosts.find(host_idx) != fail_hosts.end())
        {
            continue;
        }

        host = data_->GetMaster(host_idx);
        if (!host)
        {
            continue;
        }

        history_hosts.insert(host_idx);

        return host;
    }

    return NULL;
}

Host* HostGroup::Slave(size_t hash, int retry_cnt)
{
    if (0 == data_->slave_hosts_cnt)
    {
        return NULL;
    }

    uint32_t legal_idx = hash % data_->slave_hosts_cnt;
    uint32_t start_idx = legal_idx;

    if (retry_cnt > 0)
    {
        start_idx = legal_idx + retry_cnt;
    }

    for (uint32_t i = start_idx; i < start_idx + data_->slave_hosts_cnt; i++)
    {
        uint32_t idx = i % data_->slave_hosts_cnt;
        if (data_->slave_hosts[idx].fail_num() == 0)
        {
            return data_->GetSlave(idx);
        }
    }

    for (uint32_t i = legal_idx; i < legal_idx + data_->slave_hosts_cnt; i++)
    {
        uint32_t idx = i % data_->slave_hosts_cnt;
        if (data_->slave_hosts[idx].CheckAlive(server_, true))
        {
            return data_->GetSlave(idx);
        }
    }

    return NULL;
}

Host* HostGroup::GetMasterHost(size_t hash)
{
    if (0 == data_->master_hosts_cnt)
    {
        return NULL;
    }

    auto index = (hash % data_->master_hosts_cnt);

    return data_->GetMaster(index);
}

Host* HostGroup::GetMasterHostByIndex(size_t index)
{
    return data_->GetMaster(index);
}

int HostGroup::GetAliveHostCount(bool strict)
{
    int ret = 0;

    for (auto& host : hosts_)
    {
        if (host.second->CheckAlive(server_, strict, false))
        {
            ret++;
        }
    }

    return ret;
}

struct HostGroupProvider::Data
{
    mutable pthread_mutex_t mutex;
    uint16_t avail_group_cnt;
    uint16_t next_group_index;
    HostGroupData group_data[80];

    Data()
        : avail_group_cnt(arraysize(group_data)),
        next_group_index(0)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setrobust_np(&attr, PTHREAD_MUTEX_ROBUST_NP);

        pthread_mutex_init(&mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~Data()
    {
        pthread_mutex_destroy(&mutex);
    }

    int GetIdleIndex()
    {
        if (avail_group_cnt > 0)
        {
            while (1)
            {
                int index = next_group_index++;
                if (next_group_index == arraysize(group_data))
                {
                    next_group_index %= arraysize(group_data);
                }

                if (group_data[index].index == -1)
                {
                    avail_group_cnt--;

                    return index;
                }
            }
        }
        else
        {
            return -1;
        }
    }

    HostGroup* Create(Server* server)
    {
        HostGroupData* data = NULL;
        if (g_process == SHS_PROCESS_WORKER)
        {
            data = new HostGroupData();
            data->index = -1;
            data->ref_cnt = 1;
            data->generation = g_process_generation;
            data->group = new HostGroup(server, data);
            return data->group;
        }
        
        SharedMemoryScopedLock lock(mutex);
        int index = GetIdleIndex();

        if (index == -1)
        {
            data = new HostGroupData();
        }
        else
        {
            data = group_data + index;
        }

        data->index = index;
        data->ref_cnt = FLAGS_num_processes + 1 + 1;
        data->generation = g_process_generation;

        data->group = new HostGroup(server, data);

        return data->group;
    }

    void Release(HostGroup* group)
    {
        SharedMemoryScopedLock lock(mutex);
        boost::scoped_ptr<HostGroup> guard(group);
        if (group->data()->group != group)
        {
            return ;
        }

        if (group->data()->index == -1)
        {
            delete group->data();
        }
        else
        {
            group->data()->ref_cnt -= 1;
            if (group->data()->ref_cnt > 0)
            {
                return;
            }
            group->data()->Reset();
            avail_group_cnt++;
        }
    }

    void Check(const HostGroupProvider::CheckCallback& cb)
    {
        SharedMemoryScopedLock lock(mutex);

        for (size_t i = 0;i < arraysize(group_data); i++)
        {
            if (group_data[i].generation != g_process_generation)
            {
                continue;
            }

            if (group_data[i].group == NULL)
            {
                continue;
            }

            group_data[i].group->Check(cb);
        }
    }

    std::vector<HostGroup*> GetAvailHostGroups() const
    {
        std::vector<HostGroup*> v;
        {
            SharedMemoryScopedLock lock(mutex);

            for (size_t i = 0;i < arraysize(group_data); i++)
            {
                if (group_data[i].generation != g_process_generation 
                    && group_data[i].index != -1)
                {
                    continue;
                }

                if (group_data[i].group)
                {
                    v.push_back(group_data[i].group);
                }
            }
        }

        return v;
    }
};

HostGroupProvider* HostGroupProvider::instance()
{
    return Singleton<HostGroupProvider>::instance();
}

HostGroup* HostGroupProvider::Create(Server* server)
{
    return data_->Create(server);
}

void HostGroupProvider::Release(HostGroup* group)
{
    data_->Release(group);
}

void HostGroupProvider::Check(const CheckCallback& cb)
{
    data_->Check(cb);
}

std::vector<HostGroup*> HostGroupProvider::GetAvailHostGroups() const
{
    return data_->GetAvailHostGroups();
}

HostGroupProvider::HostGroupProvider()
    : region_(boost::interprocess::anonymous_shared_memory(sizeof(Data)))
    , data_(new (region_.get_address()) Data())
{
}

HostGroupProvider::~HostGroupProvider()
{
}

} // namespace downstream
} // namespace shs
