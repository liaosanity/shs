#include "log_buffer.h"

namespace shs 
{

template<int SIZE>
const char* LogBuffer<SIZE>::DebugString()
{
    *current_ = '\0';

    return data_;
}

template<int SIZE>
void LogBuffer<SIZE>::CookieStart() {}
template<int SIZE> void LogBuffer<SIZE>::CookieEnd() {}

template class LogBuffer<kSmallBuffer>;
template class LogBuffer<kLargeBuffer>;

} // namespace shs
