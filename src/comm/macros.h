#pragma once

#include <stddef.h>

namespace shs 
{

#define DISALLOW_COPY(TypeName) \
  TypeName(const TypeName&) = delete

#define DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&) = delete

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

template <typename T, size_t N> char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

template<typename T>
inline void ignore_result(const T&) {}

#undef LIKELY
#undef UNLIKELY

#define LIKELY(x)   (__builtin_expect(!!(x), 0))
#define UNLIKELY(x) (__builtin_expect((x), 0))

} // namespace shs
