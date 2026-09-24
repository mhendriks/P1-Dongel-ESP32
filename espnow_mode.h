#pragma once
#include <stdint.h>

enum class EspNowMode : uint8_t { off = 0, nrg_monitor = 1, modbus_slave_sink = 2 };
extern EspNowMode espNowMode;
