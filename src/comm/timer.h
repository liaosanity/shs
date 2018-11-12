#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <cstddef>
#include <map>
#include <boost/shared_ptr.hpp>

namespace shs 
{

class QTimer
{
public:
    QTimer(bool auto_start = false);
    ~QTimer();

    void Start();
    void Stop();
    void Reset();
    void Continue();
    double Elapsed(uint64_t *microseconds = NULL);

private:
    uint64_t start_;
    uint64_t end_;
    bool     active_;
};

class QTimerFactory
{
public:
    QTimerFactory();
    ~QTimerFactory();

    std::map<std::string, boost::shared_ptr<QTimer> > GetTimers();
    boost::shared_ptr<QTimer> Timer(const std::string& name);

private:
    std::map<std::string, boost::shared_ptr<QTimer> > timers_;
};

} // namespace shs

#endif
