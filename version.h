#pragma once

#define _VERSION_MAJOR 4
#define _VERSION_MINOR 17
#define _VERSION_PATCH 1
#define _VERSION_ONLY STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "." STR(_VERSION_PATCH)

#define STR1(x) #x
#define STR(x) STR1(x)

static inline uint32_t packed_version_u32() {
  return ( (uint32_t(_VERSION_MAJOR) << 16) | (uint32_t(_VERSION_MINOR) << 8) | uint32_t(_VERSION_PATCH) );
}