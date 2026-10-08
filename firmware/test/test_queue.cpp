#include <atomic>
#include <thread>

#include "../star_tracker/src/core/step_queue.h"
#include "minitest.h"

using namespace st;

TEST(queue_fifo_and_capacity) {
  StepQueue<8> q;
  CHECK(q.empty());
  for (int i = 0; i < 8; ++i) CHECK(q.push(StepEvent{i, 1}));
  CHECK(q.full());
  CHECK(!q.push(StepEvent{99, 1}));
  StepEvent e;
  for (int i = 0; i < 8; ++i) {
    CHECK(q.peek(&e));
    CHECK_EQ(e.t_us, int64_t(i));
    q.pop();
  }
  CHECK(!q.peek(&e));
}

TEST(queue_survives_index_wraparound_and_threads) {
  // Producer/consumer on two threads, as with loop() and the ISR.
  StepQueue<64> q;
  constexpr int kN = 200000;
  std::atomic<bool> ok{true};
  std::thread consumer([&] {
    int64_t expect = 0;
    StepEvent e;
    while (expect < kN) {
      if (q.peek(&e)) {
        if (e.t_us != expect) ok = false;
        q.pop();
        ++expect;
      }
    }
  });
  for (int64_t i = 0; i < kN;) {
    if (q.push(StepEvent{i, int8_t(i % 2 ? 1 : -1)})) ++i;
  }
  consumer.join();
  CHECK(ok.load());
  CHECK(q.empty());
}
