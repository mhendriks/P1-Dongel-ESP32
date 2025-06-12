#pragma once

#define _VERSION_MAJOR 4
#define _VERSION_MINOR 14
#define _VERSION_PATCH 0

#define __MON__ ((__DATE__[0] + __DATE__[1] + __DATE__[2]) == 281   ? "01" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 269 ? "02" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 288 ? "03" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 291 ? "04" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 295 ? "05" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 301 ? "06" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 299 ? "07" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 285 ? "08" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 296 ? "09" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 294 ? "10" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 307 ? "11" \
                 : (__DATE__[0] + __DATE__[1] + __DATE__[2]) == 268 ? "12" \
                                                                    : "00")

#define STR1(x) #x
#define STR(x) STR1(x)
#define DAY_STR __DATE__[4], __DATE__[5]
#define MONTH_STR __MON__[0], __MON__[1]
#define YEAR_STR __DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10]

#define _VERSION_ONLY STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "." STR(_VERSION_PATCH)

const char compile_date[] = { DAY_STR, '/', MONTH_STR, '/', YEAR_STR, '\0' };

class FW {
public:
  FW() {
    sprintf(Version, "%s (%s)", _VERSION_ONLY, compile_date);
  }
  char Version[20];

private:
} Firmware;