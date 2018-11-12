#ifndef INVOKE_PARAMS_H
#define INVOKE_PARAMS_H

#include <stdint.h>
#include <string>
#include <map>

#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace shs 
{

class InvokeTimer;

class InvokeParams 
{
public:
    InvokeParams();
    virtual ~InvokeParams() {}; 

    void set_request_time(double t);
    void set_enqueue_time(double t);
    void set_dequeue_time(double t);
    void set_client_ip(const std::string& ip);
    void set_client_port(uint16_t port);
    void set_task_queue_size(size_t size);
    void set_timer_queue_size(size_t size);

    double get_request_time() const { return request_time_; }
    double get_enqueue_time() const { return enqueue_time_; }
    double get_dequeue_time() const { return dequeue_time_; }
    const std::string& get_client_ip() const { return client_ip_; }
    uint16_t get_client_port() const { return client_port_; }
    size_t get_task_queue_size() const { return task_queue_size_; }
    size_t get_timer_queue_size() const { return timer_queue_size_; }

    boost::shared_ptr<InvokeTimer> get_timer() 
    {
        return timer_.lock();
    }

    void set_timer(boost::shared_ptr<InvokeTimer> timer)
    {
        timer_ = timer;
    }

protected:
    double      request_time_;
    double      enqueue_time_;
    double      dequeue_time_;
    std::string client_ip_;
    uint16_t    client_port_;
    size_t      task_queue_size_;
    size_t      timer_queue_size_;

    boost::weak_ptr<InvokeTimer> timer_;
};

} // namespace shs

#endif // INVOKE_PARAMS_H
