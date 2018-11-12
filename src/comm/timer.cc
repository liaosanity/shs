#include "timer.h"

#include <sys/time.h>

namespace shs 
{

using namespace std;

static uint64_t get_monotonic_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);                                                                                                                                                                                                          
    return (tv.tv_sec * 1000000 + tv.tv_usec);
}

QTimer::QTimer(bool auto_start)
    : start_(0)
    , end_(0)
    , active_(false)
{
    if (auto_start)
    {
        Start();
    }
}

QTimer::~QTimer()
{
}

void QTimer::Start()
{
    active_ = true;
    start_ = get_monotonic_time(); 
}

void QTimer::Stop()
{
    active_ = false;
    end_ = get_monotonic_time();
}

void QTimer::Reset()
{
    start_ = get_monotonic_time();
}

void QTimer::Continue()
{
    uint64_t elapsed;

    if (active_)
    {
        return;
    } 

    elapsed = end_ - start_;
    start_ = get_monotonic_time();
    start_ -= elapsed; 
    active_ = true;
}

double QTimer::Elapsed(uint64_t *microseconds)
{
    double   total;
    uint64_t elapsed;

    if (active_)
    {
        end_ = get_monotonic_time();
    }

    elapsed = end_ - start_;
    total = elapsed / 1e6;

    if (microseconds != NULL)
    {
        *microseconds = elapsed % 1000000;
    }

    return total;
}

QTimerFactory::QTimerFactory()
{
}

QTimerFactory::~QTimerFactory()
{
    timers_.clear();
}

boost::shared_ptr<QTimer> QTimerFactory::Timer(const std::string& name)
{
    map<string, boost::shared_ptr<QTimer> >::iterator iter = timers_.find(name);
    if (iter != timers_.end())
    {
        return iter->second;
    }

    boost::shared_ptr<QTimer> timer(new QTimer()); 
    timers_[name] = timer;

    return timer;
}

std::map<std::string, boost::shared_ptr<QTimer> > QTimerFactory::GetTimers()
{
    return timers_;
}

} // namespace shs
