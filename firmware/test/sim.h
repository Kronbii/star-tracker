// Host-side executor: plays the role of the step ISR so tests can run the
// controller against simulated time, including a simulated home switch.
#pragma once

#include <cstdint>
#include <vector>

#include "../star_tracker/src/core/motion.h"

namespace sim {

struct Record {
  int64_t t_us;
  int32_t raw_after;
  int8_t dir;
};

class Executor {
 public:
  explicit Executor(st::MotionController& c) : c_(c) {}

  // Home switch: pressed while executed raw position <= switch_raw.
  void setSwitch(bool present, int32_t switch_raw) {
    switch_present_ = present;
    switch_raw_ = switch_raw;
  }

  bool switchPressed() const { return switch_present_ && raw_ <= switch_raw_; }

  // Advance simulated time to `until_us` in loop iterations of `loop_us`,
  // queueing `horizon_us` ahead like the firmware does.
  void run(int64_t until_us, int64_t loop_us = 1000, int64_t horizon_us = 20000) {
    while (now_ < until_us) {
      now_ += loop_us;
      if (now_ > until_us) now_ = until_us;
      // Execute queued events that are due (the ISR).
      while (!queue_.empty() && queue_.front().t_us <= now_) {
        const st::StepEvent e = queue_.front();
        queue_.erase(queue_.begin());
        raw_ += e.dir;
        log.push_back({e.t_us, raw_, e.dir});
      }
      // Main loop: feed the switch, then refill the queue.
      c_.updateSwitch(switchPressed(), now_, raw_);
      while (queue_.size() < 256 && c_.peekTime() <= now_ + horizon_us) {
        st::StepEvent e;
        c_.next(&e);
        queue_.push_back(e);
      }
    }
  }

  int64_t now() const { return now_; }
  void setNow(int64_t t) { now_ = t; }
  int32_t raw() const { return raw_; }
  size_t queued() const { return queue_.size(); }

  std::vector<Record> log;

 private:
  st::MotionController& c_;
  std::vector<st::StepEvent> queue_;
  int64_t now_ = 0;
  int32_t raw_ = 0;
  bool switch_present_ = false;
  int32_t switch_raw_ = 0;
};

}  // namespace sim
