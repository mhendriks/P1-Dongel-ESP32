#pragma once

#define P1_STR1(x) #x
#define P1_STR(x) P1_STR1(x)

#define _VERSION_MAJOR 5
#define _VERSION_MINOR 9
#define _VERSION_PATCH 0
#define _VERSION_ONLY P1_STR(_VERSION_MAJOR) "." P1_STR(_VERSION_MINOR) "." P1_STR(_VERSION_PATCH)

#ifndef STR1
#define STR1(x) #x
#endif
#ifndef STR
#define STR(x) STR1(x)
#endif

static inline uint32_t packed_version_u32() {
  return ( (uint32_t(_VERSION_MAJOR) << 16) | (uint32_t(_VERSION_MINOR) << 8) | uint32_t(_VERSION_PATCH) );
}
