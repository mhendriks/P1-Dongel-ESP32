#ifndef HAN2_INCLUDE_UTIL_H
#define HAN2_INCLUDE_UTIL_H

#include <Arduino.h>

namespace han {

template<typename T, size_t N>
constexpr size_t array_size(const T (&)[N]) {
  return N;
}

}  // namespace han

#endif

