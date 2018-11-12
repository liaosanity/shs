#pragma once

#include <stdint.h>
#include <set>
#include <map>
#include <string>
#include <pthread.h>
#include <tr1/functional>
#include <boost/noncopyable.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

#include "comm/timestamp.h"
#include "comm/singleton.h"

namespace shs 
{
namespace downstream 
{

class Server;
class HostGroup;
class HostGroupData;

class Host
{
public:
    enum Result
    {
        kOk,
        kFail,
        kAbort,
        kStatusFail
    };

    Host();

    void Reset();

    void Init(const std::string& ip, uint16_t port, bool backup);

    const std::string ip() const { return ip_; }
    uint16_t port() const { return port_; }

    bool CheckAlive(Server* server, bool strict = false, bool do_skip = true);

    const std::string State(Server* server) const;
    std::string ip_port() const;

    Timestamp down_start() const { return down_start_; }
    Timestamp last_time() const { return last_time_; }
    uint32_t fail_num() const { return fail_num_; }
    void set_group_idx(int idx) { group_idx_ = idx; }
    void set_host_idx(int idx) { host_idx_ = idx; }
    int group_idx() const { return group_idx_; }
    int host_idx() const { return host_idx_; }
    bool backup() const { return backup_; }
    std::string tag() const { return tag_; }
    void gen_tag();

    void UpdateStats(Result result, Server* server);
    void SetOnline();
    void SetOffline();
    bool IsOnline() const;

private:
    bool IsHealthy(Server* server, bool strict, bool do_skip);

    char ip_[32];
    uint16_t port_;
    bool backup_; // master or slave

    mutable boost::interprocess::interprocess_mutex mutex_;
    uint32_t fail_num_;
    uint32_t status_fail_num_;

    Timestamp last_time_;
    Timestamp down_start_;

    int group_idx_;
    int host_idx_;

    char tag_[128];

    bool is_online_;

    struct Stats
    {
        uint32_t delay;
        uint32_t num_req;
        uint32_t num_rsp;
        uint32_t num_retry;
    };

    Stats history_stats_[60];
    Stats current_stats_;
    size_t last_idx_;
};

struct HostGroupData
{
    HostGroupData()
        : master_hosts_cnt(0)
        , slave_hosts_cnt(0)
        , ref_cnt(0)
        , generation(0)
        , group(NULL)
        , index(-1)
    {
    }

    Host master_hosts[2000];
    Host slave_hosts[2000];

    uint16_t master_hosts_cnt;
    uint16_t slave_hosts_cnt;

    uint16_t ref_cnt;
    uint16_t generation;

    HostGroup* group;
    int index;
    
    void Reset();

    Host* AddMaster();
    Host* AddSlave();
    Host* GetMaster(size_t index);
    Host* GetSlave(size_t index);
};

class HostGroup
{
public:
    HostGroup(Server* server, HostGroupData* data);
    ~HostGroup() {}

    void AddMasterServer(const std::vector<std::string>& hosts, uint16_t port);
    void AddSlaveServer(const std::string& ip, uint16_t port);
    void AddSlaveServer(const std::vector<std::string>& hosts, uint16_t port);

    void HashInit();

    Host* GetMasterHost(size_t hash);
    Host* GetMasterHostByIndex(size_t index);
    Host* SelectHost(size_t hash, std::set<int>& history_hosts, int host);
    size_t GetMasterHostSize() const { return data_->master_hosts_cnt; }

    int GetAliveHostCount(bool strict = false);

    HostGroupData* data() { return data_; }

    void Check(const std::tr1::function<void(Server* server)>& cb)
    {
        cb(server_);
    }

    Server* server() const { return server_; }

private:
    Host* Master(size_t hash, std::set<int>& history_hosts);
    Host* Slave(size_t hash, int retry_cnt);
    int GetPos(size_t hash);
    int GetHashIdx(int start, int end, size_t hash);

    struct VirtualNode
    {
        VirtualNode()
            : idx(0)
            , hash_offset(0)
        {}

        int idx;
        unsigned int hash_offset;
    };

    HostGroupData* data_;
    Server* server_;
    std::map<std::string, Host*> hosts_;
    std::vector<VirtualNode> virtual_node_set_;
};

class HostGroupProvider : boost::noncopyable
{
public:
    static HostGroupProvider* instance();
    typedef std::tr1::function<void(Server*)> CheckCallback;

    HostGroup* Create(Server* server);
    void Release(HostGroup* group);
  
    void Check(const CheckCallback& cb);

    std::vector<HostGroup*> GetAvailHostGroups() const;

private:
    HostGroupProvider();
    ~HostGroupProvider();
    friend class Singleton<HostGroupProvider>;

    boost::interprocess::mapped_region region_;

    struct Data;
    Data* data_;
};

class SharedMemoryScopedLock
{
public:
    SharedMemoryScopedLock(pthread_mutex_t& mutex, bool can_skip = false)
        : mutex_(&mutex)
        , valid_(true)
    {
        int ret = 0;
        if (can_skip)
        {
            ret = pthread_mutex_trylock(mutex_);
            if (ret != 0)
            {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += 5*1000*1000;
                ret = pthread_mutex_timedlock(mutex_, &ts);
            }
        }
        else
        {
            ret = pthread_mutex_lock(mutex_);
        }

        if (ret == EOWNERDEAD)
        {
            ret = pthread_mutex_consistent(mutex_);
        }

        if (ret != 0)
        {
            valid_ = false;
        }
    }

    ~SharedMemoryScopedLock()
    {
        pthread_mutex_unlock(mutex_);
    }

    bool Valid() const { return valid_; }

private:
    pthread_mutex_t* mutex_;
    bool valid_;
};

} // namespace downstream
} // namespace shs
