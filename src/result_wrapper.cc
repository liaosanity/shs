#include "result_wrapper.h"

namespace shs 
{

ResultWrapper::ResultWrapper(uint64_t id, bool ignore_stats, 
    const InvokeResult& result)
    : id_(id)
    , ignore_stats_(ignore_stats)
    , result_(result)
{
}

} // namespace shs
