#include "worker.h"

#include "log/logging.h"
#include "core/shs_epoll.h"
#include "core/shs_memory.h"

#include "event_watcher.h"
#include "task.h"

namespace shs 
{

using namespace std;
using namespace boost;

const int kDefaultQueueSize = 4096;

Worker::Worker(bool detach)
    : queue_size_(kDefaultQueueSize)
    , event_base_(NULL)
    , conn_pool_(NULL)
    , ev_timer_(NULL)
    , running_(false)
    , detach_(detach)
{
    worker_thread_ = NULL;
}

Worker::~Worker()
{
    if (NULL != task_watcher_.get())
    {
        task_watcher_->Close();
    }

    task_watcher_.reset();
    stop_watcher_.reset();
    Stop();
    Close();
}

bool Worker::Init(int32_t connection_n, int queue_size) 
{
    if (queue_size > 0) 
    {
        queue_size_ = queue_size;
    }

    worker_thread_ = (shs_thread_t *)memory_calloc(sizeof(shs_thread_t));
    if (!worker_thread_)
    {
        SLOG(ERROR) << "memory_calloc() failed";

        return false;
    }
    worker_thread_->type = THREAD_WORKER;
    worker_thread_->event_base.nevents = queue_size_ * 2; 

    if (thread_event_init(worker_thread_) != SHS_OK)
    {
        SLOG(ERROR) << "thread_event_init() failed";

        return false;
    }

    event_base_ = &worker_thread_->event_base;
    conn_pool_ = &worker_thread_->conn_pool;
    ev_timer_ = &worker_thread_->event_timer;
    
    task_watcher_.reset(new PipedEventWatcher(event_base_, 
        std::tr1::bind(&Worker::HandleTask, this)));
    if (!task_watcher_->Init()) 
    {
        Close();

        return false;
    }

    return true;
}

bool Worker::Start() 
{
    stop_watcher_.reset(new PipedEventWatcher(event_base_,
        std::tr1::bind(&Worker::HandleStop, this)));
    if (!stop_watcher_->Init()) 
    {
        return false;
    }

    thread_.reset(new boost::thread(std::tr1::bind(&Worker::Main, this)));
    if (detach_)
    {
        thread_->detach();
    }

    running_ = true;

    return true;
}

boost::thread::id Worker::id() const 
{
    return thread_->get_id();
}

void Worker::Stop() 
{
    if (running_) 
    {
        stop_watcher_->Notify();
        thread_->join();
    }
}

void Worker::Close() 
{
    if (event_base_) 
    {
        event_done(event_base_);
    }

    if (conn_pool_) 
    {
        conn_pool_free(conn_pool_);
    }

    if (worker_thread_)
    { 
        memory_free(worker_thread_, sizeof(shs_thread_t));
        worker_thread_ = NULL;
    }
}

void Worker::HandleStop() 
{
    while (!tasks_.empty())
    {
        HandleTask();
    }

    Close();
    running_ = false;
}

bool Worker::AddTask(boost::shared_ptr<Task> task) 
{
    bool need_notify = false;
    {
        boost::mutex::scoped_lock lock(tasks_mutex_);
        if (tasks_.empty()) 
        {
            need_notify = true;
        }
        else if (tasks_.size() >= (size_t)queue_size_) 
        {
            return false;
        }
        tasks_.push_back(task);
    }

    if (need_notify) 
    {
        task_watcher_->Notify();
    }

    return true;
}

void Worker::HandleTask() 
{
    std::vector<boost::shared_ptr<Task> > tasks;
    {
        boost::mutex::scoped_lock lock(tasks_mutex_);
        tasks.swap(tasks_);
    }

    size_t task_size = tasks.size();
    for (auto& task : tasks)
    {
        task->SetTaskSize(task_size--);
        task->Run();
    }
}

void Worker::Main() 
{
    while (running_)
    {
        thread_event_process(worker_thread_);
    }
}

} // namespace shs
