#pragma once

#include <stdint.h>
#include <string.h>

namespace shs 
{

template<typename T>
size_t IntToBuffer(char buffer[], T value);
size_t HexToBuffer(char buffer[], uintptr_t value);

} // namespace shs

