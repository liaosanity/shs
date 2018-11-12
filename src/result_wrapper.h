#pragma once

#include <stdint.h>

#include "types.h"

namespace shs 
{

class ResultWrapper 
{
public:
    ResultWrapper(uint64_t id, bool ignore_stats, const InvokeResult& result);
    ~ResultWrapper() {}

    uint64_t id() const { return id_; }
    InvokeResult &result() { return result_; }
    bool ignore_stats() const { return ignore_stats_; }

private:
    uint64_t id_;
    bool ignore_stats_;
    InvokeResult result_;
};

} // namespace shs
