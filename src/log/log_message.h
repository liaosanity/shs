#pragma once

#include <boost/noncopyable.hpp>
#include "log/log_stream.h"

namespace shs 
{

class LogMessage : boost::noncopyable
{
public:
    LogMessage(int level);
    ~LogMessage();

    LogStream& stream()
    {
        return stream_;
    }

private:
    LogStream stream_;
    int level_;
};

} // namespace shs
