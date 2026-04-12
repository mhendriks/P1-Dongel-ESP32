#ifndef HAN2_INCLUDE_PARSER_H
#define HAN2_INCLUDE_PARSER_H

#include <Arduino.h>
#include "fields2.h"

namespace han {

struct ParseResult {
  bool ok = false;
  String error;

  explicit operator bool() const { return ok; }
};

class HanParser {
 public:
  static ParseResult parse(HanData* data, const uint8_t* frame, size_t frame_len) {
    ParseResult result;
    if (!data) {
      result.error = "null data";
      return result;
    }
    if (!frame || frame_len == 0) {
      result.error = "empty frame";
      return result;
    }

    data->clear();
    decodeFrame(data, frame, frame_len);

    if (!data->timestamp.present() &&
        !data->identification.present() &&
        !data->power_delivered_kw.present() &&
        !data->voltage_l1_v.present()) {
      result.error = "no supported OBIS fields found";
      return result;
    }

    detectListType(data);
    result.ok = true;
    return result;
  }

 private:
  static bool obisEquals(const uint8_t* bytes, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
    return bytes[0] == a && bytes[1] == b && bytes[2] == c && bytes[3] == d && bytes[4] == e && bytes[5] == f;
  }

  static uint32_t readBe32(const uint8_t* bytes) {
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
  }

  static uint16_t readBe16(const uint8_t* bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
  }

  static void setTimestamp(fields::StringValue& target, uint16_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t minute, uint8_t second) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u%02u%02u%02u%02u%02uW",
             (unsigned)(year % 100), (unsigned)month, (unsigned)day,
             (unsigned)hour, (unsigned)minute, (unsigned)second);
    target.set(buf);
  }

  static void decodeTimestamp(const uint8_t* bytes, size_t len, HanData* data) {
    if (len < 8) return;
    setTimestamp(data->timestamp, readBe16(bytes), bytes[2], bytes[3], bytes[5], bytes[6], bytes[7]);
  }

  static void decodeCompactTimestamp(const uint8_t* bytes, size_t len, HanData* data) {
    if (len < 13) return;
    setTimestamp(data->timestamp, readBe16(&bytes[1]), bytes[3], bytes[4], bytes[6], bytes[7], bytes[8]);
  }

  static void assignString(const uint8_t* obis, const char* text, HanData* data) {
    if (obisEquals(obis, 0x01, 0x01, 0x00, 0x00, 0x05, 0xFF)) data->identification.set(text);
    if (obisEquals(obis, 0x01, 0x01, 0x60, 0x01, 0x01, 0xFF)) data->equipment_id.set(text);
  }

  static void assignU16(const uint8_t* obis, uint16_t value, HanData* data) {
    if (obisEquals(obis, 0x01, 0x01, 0x20, 0x07, 0x00, 0xFF)) data->voltage_l1_v.set((float)value);
    if (obisEquals(obis, 0x01, 0x01, 0x34, 0x07, 0x00, 0xFF)) data->voltage_l2_v.set((float)value);
    if (obisEquals(obis, 0x01, 0x01, 0x48, 0x07, 0x00, 0xFF)) data->voltage_l3_v.set((float)value);
  }

  static void assignU32(const uint8_t* obis, uint32_t value, HanData* data) {
    if (obisEquals(obis, 0x01, 0x01, 0x01, 0x07, 0x00, 0xFF)) data->power_delivered_kw.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x02, 0x07, 0x00, 0xFF)) data->power_returned_kw.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x03, 0x07, 0x00, 0xFF)) data->reactive_power_delivered_kvar.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x04, 0x07, 0x00, 0xFF)) data->reactive_power_returned_kvar.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x1F, 0x07, 0x00, 0xFF)) data->current_l1_a.set(value / 100.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x33, 0x07, 0x00, 0xFF)) data->current_l2_a.set(value / 100.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x47, 0x07, 0x00, 0xFF)) data->current_l3_a.set(value / 100.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x01, 0x08, 0x00, 0xFF)) data->energy_delivered_total_kwh.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x02, 0x08, 0x00, 0xFF)) data->energy_returned_total_kwh.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x03, 0x08, 0x00, 0xFF)) data->reactive_energy_delivered_total_kvarh.set(value / 1000.0f);
    if (obisEquals(obis, 0x01, 0x01, 0x04, 0x08, 0x00, 0xFF)) data->reactive_energy_returned_total_kvarh.set(value / 1000.0f);
  }

  static void decodeFrame(HanData* data, const uint8_t* frame, size_t frame_len) {
    for (size_t i = 0; i + 8 < frame_len; ++i) {
      if (!data->timestamp.present() && frame[i] == 0x0C && i + 12 < frame_len) {
        decodeCompactTimestamp(&frame[i], frame_len - i, data);
      }

      if (frame[i] == 0x09 && frame[i + 1] == 0x06) {
        const uint8_t* obis = &frame[i + 2];
        const uint8_t type = frame[i + 8];
        const size_t value_index = i + 9;

        if (obisEquals(obis, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF) &&
            type == 0x09 && value_index < frame_len) {
          const uint8_t len = frame[value_index];
          if (value_index + 1 + len <= frame_len) decodeTimestamp(&frame[value_index + 1], len, data);
          continue;
        }

        if (type == 0x06 && value_index + 4 <= frame_len) {
          assignU32(obis, readBe32(&frame[value_index]), data);
          continue;
        }

        if (type == 0x12 && value_index + 2 <= frame_len) {
          assignU16(obis, readBe16(&frame[value_index]), data);
          continue;
        }

        if ((type == 0x0A || type == 0x09) && value_index < frame_len) {
          const uint8_t len = frame[value_index];
          if (value_index + 1 + len > frame_len) continue;
          char text[40];
          const size_t copy_len = (len < sizeof(text) - 1) ? len : sizeof(text) - 1;
          memcpy(text, &frame[value_index + 1], copy_len);
          text[copy_len] = '\0';
          assignString(obis, text, data);
        }
      }
    }
  }

  static void detectListType(HanData* data) {
    if (data->energy_delivered_total_kwh.present() ||
        data->energy_returned_total_kwh.present() ||
        data->reactive_energy_delivered_total_kvarh.present() ||
        data->reactive_energy_returned_total_kvarh.present() ||
        data->timestamp.present()) {
      data->list_type = HanListType::List2;
      return;
    }
    data->list_type = HanListType::List1;
  }
};

}  // namespace han

#endif

