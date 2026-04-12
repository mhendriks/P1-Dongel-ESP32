#ifndef HAN2_INCLUDE_TEST_H
#define HAN2_INCLUDE_TEST_H

#include "reader2.h"

namespace han {

struct HanTestFrame {
  const uint8_t* data = nullptr;
  size_t length = 0;
  HanListType type = HanListType::Unknown;
};

class HanReplayDriver {
 public:
  HanReplayDriver() = default;

  void configure(HanTestFrame list1, HanTestFrame list2,
                 unsigned long list1_interval_ms = 5000,
                 unsigned long list2_interval_ms = 20000) {
    list1_ = list1;
    list2_ = list2;
    list1_interval_ms_ = list1_interval_ms;
    list2_interval_ms_ = list2_interval_ms;
    last_list1_ms_ = 0;
    last_list2_ms_ = 0;
  }

  bool step(HanReader& reader, unsigned long now_ms) {
    if (list2_.data && list2_.length > 0 &&
        (last_list2_ms_ == 0 || (now_ms - last_list2_ms_) >= list2_interval_ms_)) {
      last_list2_ms_ = now_ms;
      last_list1_ms_ = now_ms;
      return reader.feedFrame(list2_.data, list2_.length);
    }

    if (list1_.data && list1_.length > 0 &&
        (last_list1_ms_ == 0 || (now_ms - last_list1_ms_) >= list1_interval_ms_)) {
      last_list1_ms_ = now_ms;
      return reader.feedFrame(list1_.data, list1_.length);
    }

    return false;
  }

 private:
  HanTestFrame list1_;
  HanTestFrame list2_;
  unsigned long list1_interval_ms_ = 5000;
  unsigned long list2_interval_ms_ = 20000;
  unsigned long last_list1_ms_ = 0;
  unsigned long last_list2_ms_ = 0;
};

}  // namespace han

#include "test_kamstrup_nve.h"

#endif
