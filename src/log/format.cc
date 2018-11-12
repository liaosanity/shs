#include "format.h"

#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <boost/type_traits/is_arithmetic.hpp>

namespace shs 
{

#pragma GCC diagnostic ignored "-Wtype-limits"
const char digits[] = "9876543210123456789";
const char* zero = digits + 9;
static_assert(sizeof(digits) == 20, "digits size not equal 20");

const char digitsHex[] = "0123456789ABCDEF";
static_assert(sizeof(digitsHex) == 17, "digitsHex size not equal 17");

template<typename T>
size_t IntToBuffer(char buffer[], T value)
{
    T i = value;
    char* p = buffer;

    do
    {
        int lsd = static_cast<int>(i % 10);
        i /= 10;
        *p++ = zero[lsd];
    } while (i != 0);

    if (value < 0)
    {
        *p++ = '-';
    }

    *p = '\0';
    std::reverse(buffer, p);

    return p - buffer;
}

size_t HexToBuffer(char buffer[], uintptr_t value)
{
    uintptr_t i = value;
    char* p = buffer;

    do
    {
        int lsd = i % 16;
        i /= 16;
        *p++ = digitsHex[lsd];
    } while (i != 0);

    *p = '\0';
    std::reverse(buffer, p);

    return p - buffer;
}

template size_t IntToBuffer(char [], char);
template size_t IntToBuffer(char [], short);
template size_t IntToBuffer(char [], unsigned short);
template size_t IntToBuffer(char [], int);
template size_t IntToBuffer(char [], unsigned int);
template size_t IntToBuffer(char [], long);
template size_t IntToBuffer(char [], unsigned long);
template size_t IntToBuffer(char [], long long);
template size_t IntToBuffer(char [], unsigned long long);

} // namespace shs
