#pragma once 

#include <vector>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include "core/shs_types.h"
#include "core/shs_thread.h"
#include "core/shs_conn_pool.h"

namespace shs 
{

class Task;
class PipedEventWatcher;

class Worker
{
public:
    Worker(bool detach = false);
    ~Worker();

    bool Init(int32_t connection_n, int queue_size = 0);
    bool Start();
    void Stop();
    void Close();

    boost::thread::id id() const;

    bool AddTask(boost::shared_ptr<Task> task);

    event_base_t *event_base() { return event_base_; }
    conn_pool_t *conn_pool() { return conn_pool_; }
    event_timer_t *event_timer() { return ev_timer_; }

private:
    void Main();
    void HandleStop();
    void HandleTask();

    int queue_size_;
    event_base_t *event_base_;
    conn_pool_t *conn_pool_;
    event_timer_t *ev_timer_;
    boost::scoped_ptr<PipedEventWatcher> task_watcher_;

    volatile bool running_;
    boost::scoped_ptr<PipedEventWatcher> stop_watcher_;

    std::vector<boost::shared_ptr<Task> > tasks_;

    bool detach_;
    boost::mutex tasks_mutex_;
    boost::shared_ptr<boost::thread> thread_;
    shs_thread_t *worker_thread_;
};

} // namespace shs
