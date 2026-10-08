// Single-producer / single-consumer ring buffer of step events.
//
// Producer: the main loop (fills a few milliseconds ahead).
// Consumer: the step timer ISR. All methods are header-only and inline so the
// ISR copy lands in IRAM with the ISR; no locks, no allocation, no floats.
#pragma once

#include <atomic>
#include <cstdint>

#include "motion.h"

namespace st {

template <uint32_t N>
class StepQueue {
  static_assert((N & (N - 1)) == 0, "capacity must be a power of two");

 public:
  inline bool push(const StepEvent& e) {
    const uint32_t h = head_.load(std::memory_order_relaxed);
    const uint32_t t = tail_.load(std::memory_order_acquire);
    if (h - t >= N) return false;
    buf_[h & (N - 1)] = e;
    head_.store(h + 1, std::memory_order_release);
    return true;
  }
  inline bool peek(StepEvent* e) const {
    const uint32_t t = tail_.load(std::memory_order_relaxed);
    const uint32_t h = head_.load(std::memory_order_acquire);
    if (h == t) return false;
    *e = buf_[t & (N - 1)];
    return true;
  }
  inline void pop() {
    const uint32_t t = tail_.load(std::memory_order_relaxed);
    tail_.store(t + 1, std::memory_order_release);
  }
  inline uint32_t size() const {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
  }
  inline bool full() const { return size() >= N; }
  inline bool empty() const { return size() == 0; }
  static constexpr uint32_t capacity() { return N; }

 private:
  StepEvent buf_[N]{};
  std::atomic<uint32_t> head_{0};
  std::atomic<uint32_t> tail_{0};
};

}  // namespace st
