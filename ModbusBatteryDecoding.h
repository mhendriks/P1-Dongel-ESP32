#pragma once

#include <stdint.h>

// eModbus exposes each Modbus register as a host uint16_t.  These helpers
// make signedness, scaling, invalid sentinels and multiword ordering explicit
// in profiles rather than hiding them in a connector implementation.
inline int16_t decodeModbusS16(uint16_t raw) {
  return static_cast<int16_t>(raw);
}

inline float scaleModbusValue(int32_t raw, float scale) {
  return static_cast<float>(raw) * scale;
}

inline uint32_t decodeModbusU32(uint16_t firstWord, uint16_t secondWord, bool wordsSwapped) {
  return wordsSwapped
      ? (static_cast<uint32_t>(secondWord) << 16) | firstWord
      : (static_cast<uint32_t>(firstWord) << 16) | secondWord;
}

inline bool isModbusInvalid(uint16_t raw, uint16_t invalidValue, bool hasInvalidValue) {
  return hasInvalidValue && raw == invalidValue;
}
