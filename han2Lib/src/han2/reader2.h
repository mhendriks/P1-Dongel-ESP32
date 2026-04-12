#ifndef HAN2_INCLUDE_READER_H
#define HAN2_INCLUDE_READER_H

#include <Arduino.h>
#include <type_traits>
#include "parser2.h"
#include "compat_dsmr2.h"

namespace han {

class HanReader {
 public:
  explicit HanReader(Stream* stream, size_t max_frame_len = 768)
      : stream_(stream), max_frame_len_(max_frame_len) {
    clearAll();
  }

  void doChecksum(bool checksum) { (void)checksum; }

  void enable(bool once = true) {
    (void)once;
    enabled_ = true;
  }
  void disable() { enabled_ = false; clear(); }

  bool available() const { return available_; }
  size_t frameLength() const { return frame_len_; }
  const uint8_t* frame() const { return frame_; }

  String rawHex() const {
    String out;
    for (size_t i = 0; i < frame_len_; ++i) {
      if (i) out += ' ';
      char buf[4];
      snprintf(buf, sizeof(buf), "%02X", frame_[i]);
      out += buf;
    }
    return out;
  }

  String CompleteRaw() const {
    return rawHex();
  }

  String raw() const {
    return rawHex();
  }

  bool loop() {
    if (!enabled_ || !stream_) return false;

    while (stream_->available() > 0) {
      int c = stream_->read();
      if (c < 0) return available_;
      if (handleByte((uint8_t)c)) return true;
    }

    return available_;
  }

  bool feedByte(uint8_t b) {
    return handleByte(b);
  }

  bool feedBytes(const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    for (size_t i = 0; i < len; ++i) {
      if (handleByte(data[i])) return true;
    }
    return available_;
  }

  bool feedFrame(const uint8_t* data, size_t len) {
    clear();
    collecting_ = false;
    return feedBytes(data, len);
  }

  bool parse(HanData* data, String* err = nullptr) {
    ParseResult result = HanParser::parse(data, frame_, frame_len_);
    if (!result.ok && err) *err = result.error;
    if (result.ok) state_.update(*data);
    clear();
    return result.ok;
  }

  template<typename TDsmrData>
  typename std::enable_if<!std::is_same<TDsmrData, HanData>::value, bool>::type
  parse(TDsmrData* data, String* err = nullptr) {
    HanData intermediate;
    ParseResult result = HanParser::parse(&intermediate, frame_, frame_len_);
    if (!result.ok && err) *err = result.error;
    if (result.ok) {
      state_.update(intermediate);
      compat::mapToDsmr(*data, state_, String(), "HAN reader");
    }
    clear();
    return result.ok;
  }

  const HanState& state() const { return state_; }

  void clear() {
    frame_len_ = 0;
    available_ = false;
    overflow_ = false;
  }

  void clearAll() {
    clear();
    collecting_ = false;
    enabled_ = true;
    state_.clear();
  }

  void changeStream(Stream* stream) { stream_ = stream; }
  void ChangeStream(Stream* stream) { changeStream(stream); }

 private:
  bool handleByte(uint8_t b) {
    if (b == 0x7E) {
      if (collecting_ && frame_len_ > 1) {
        appendByte(0x7E);
        available_ = true;
        collecting_ = false;
        return true;
      }

      clear();
      collecting_ = true;
      appendByte(0x7E);
      return false;
    }

    if (!collecting_) return false;
    appendByte(b);
    return available_;
  }

  void appendByte(uint8_t b) {
    if (frame_len_ >= max_frame_len_ || frame_len_ >= sizeof(frame_)) {
      overflow_ = true;
      collecting_ = false;
      clear();
      return;
    }
    frame_[frame_len_++] = b;
  }

  Stream* stream_ = nullptr;
  size_t max_frame_len_ = 768;
  uint8_t frame_[768];
  size_t frame_len_ = 0;
  bool enabled_ = true;
  bool collecting_ = false;
  bool available_ = false;
  bool overflow_ = false;
  HanState state_;
};

}  // namespace han

#endif
