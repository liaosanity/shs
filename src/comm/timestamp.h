#pragma once

#include <boost/operators.hpp>

namespace shs 
{

class Timestamp : boost::less_than_comparable<Timestamp>
{
public:
    static const int kMicroSecondsPerSecond = 1000 * 1000;

    Timestamp()
        : microseconds_since_epoch_(0) {}

    explicit Timestamp(int64_t ms)
        : microseconds_since_epoch_(ms) {}

    void swap(Timestamp& other)
    {
        std::swap(microseconds_since_epoch_, 
            other.microseconds_since_epoch_);
    }

    bool Valid() const { return microseconds_since_epoch_ > 0; }

    time_t SecondsSinceEpoch() const
    {
        return static_cast<time_t>(
            microseconds_since_epoch_ / kMicroSecondsPerSecond);
    }

    int64_t MicroSecondsSinceEpoch() const
    {
        return microseconds_since_epoch_;
    }

    static Timestamp Now();
    static Timestamp Invalid()
    {
        return Timestamp();
    }

    void AsTimeval(struct timeval* tv)
    {
        tv->tv_sec = microseconds_since_epoch_/1000;
        tv->tv_usec = (microseconds_since_epoch_ % 1000) * 1000;
    }

    std::string ToString() const;

    static std::string LocalTimeToString(time_t t);

private:
    int64_t microseconds_since_epoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.MicroSecondsSinceEpoch() < rhs.MicroSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.MicroSecondsSinceEpoch() == rhs.MicroSecondsSinceEpoch();
}

inline int64_t TimeDifference(Timestamp high, Timestamp low)
{
    return (high.MicroSecondsSinceEpoch() - low.MicroSecondsSinceEpoch());
}

inline Timestamp AddTime(Timestamp timestamp, int64_t microseconds)
{
    return Timestamp(timestamp.MicroSecondsSinceEpoch() + microseconds);
}

} // namespace shs
