#include <vector>

#include "../star_tracker/src/core/intervalometer.h"
#include "minitest.h"

using namespace st;

namespace {

struct Edge {
  int64_t t_ms;
  bool shutter;
  bool focus;
};

// Samples the outputs every `step_ms` and records changes.
std::vector<Edge> runAndRecord(Intervalometer& iv, int64_t until_ms, int64_t step_ms) {
  std::vector<Edge> e;
  bool s = iv.shutterClosed(), f = iv.focusClosed();
  e.push_back({0, s, f});
  for (int64_t t = 0; t <= until_ms; t += step_ms) {
    iv.update(t * 1000);
    if (iv.shutterClosed() != s || iv.focusClosed() != f) {
      s = iv.shutterClosed();
      f = iv.focusClosed();
      e.push_back({t, s, f});
    }
  }
  return e;
}

}  // namespace

TEST(plan_total_time) {
  ShootPlan p;
  p.frames = 10;
  p.exposure_ms = 60000;
  p.gap_ms = 3000;
  p.delay_ms = 2000;
  p.prefocus_ms = 200;
  CHECK_EQ(p.totalMs(), uint64_t(2000 + 10 * 60200 + 9 * 3000));
  CHECK(p.valid());
  p.frames = 0;
  CHECK(!p.valid());
}

TEST(bulb_sequence_timing_is_exact) {
  ShootPlan p;
  p.frames = 3;
  p.exposure_ms = 1000;
  p.gap_ms = 500;
  p.delay_ms = 300;
  p.prefocus_ms = 100;
  Intervalometer iv;
  CHECK(iv.start(p, 0));
  const auto e = runAndRecord(iv, 6000, 1);
  // Expected: focus at 300, shutter 400..1400, focus off at 1400; next frame
  // focus 1900, shutter 2000..3000; then 3500, 3600..4600; done.
  int shutter_on = 0;
  for (size_t i = 1; i < e.size(); ++i) {
    if (e[i].shutter && !e[i - 1].shutter) {
      ++shutter_on;
      // find the release
      for (size_t j = i + 1; j < e.size(); ++j) {
        if (!e[j].shutter) {
          CHECK_EQ(e[j].t_ms - e[i].t_ms, int64_t(1000));
          break;
        }
      }
    }
  }
  CHECK_EQ(shutter_on, 3);
  CHECK_EQ(e[1].t_ms, int64_t(300));  // focus first
  CHECK(e[1].focus && !e[1].shutter);
  CHECK_EQ(e[2].t_ms, int64_t(400));  // then shutter
  CHECK(e[2].focus && e[2].shutter);
  CHECK(iv.state() == ShotState::Done);
  CHECK_EQ(iv.framesDone(), 3);
  CHECK(!iv.shutterClosed() && !iv.focusClosed());
}

TEST(late_updates_do_not_stretch_exposures) {
  ShootPlan p;
  p.frames = 5;
  p.exposure_ms = 2000;
  p.gap_ms = 1000;
  p.delay_ms = 0;
  p.prefocus_ms = 0;
  Intervalometer iv;
  CHECK(iv.start(p, 0));
  // Update only every 7 ms: transitions still land on the 1 ms grid of the plan.
  int frames = 0;
  for (int64_t t = 0; t < 20000; t += 7) {
    iv.update(t * 1000);
    if (iv.frameJustCompleted()) ++frames;
  }
  CHECK_EQ(frames, 5);
  CHECK_EQ(iv.framesDone(), 5);
  CHECK(iv.state() == ShotState::Done);
}

TEST(camera_timed_mode_pulses) {
  ShootPlan p;
  p.frames = 2;
  p.exposure_ms = 1000;
  p.gap_ms = 200;
  p.delay_ms = 0;
  p.prefocus_ms = 0;
  p.bulb = false;
  p.pulse_ms = 150;
  Intervalometer iv;
  CHECK(iv.start(p, 0));
  const auto e = runAndRecord(iv, 3000, 1);
  CHECK_EQ(e.size(), size_t(4));  // on, off@150, on@1200, off@1350
  CHECK(e[0].shutter);  // starts immediately with no delay/prefocus
  CHECK(!e[1].shutter);
  CHECK_EQ(e[1].t_ms, int64_t(150));
  CHECK(e[2].shutter);
  CHECK_EQ(e[2].t_ms, int64_t(1200));
}

TEST(abort_releases_outputs) {
  ShootPlan p;
  p.frames = 10;
  p.exposure_ms = 10000;
  p.delay_ms = 0;
  p.prefocus_ms = 0;
  Intervalometer iv;
  CHECK(iv.start(p, 0));
  iv.update(1000 * 1000);
  CHECK(iv.shutterClosed());
  CHECK(iv.remainingMs(1000 * 1000) > 0);
  iv.abort();
  CHECK(!iv.shutterClosed() && !iv.focusClosed());
  CHECK(!iv.active());
  CHECK_EQ(iv.remainingMs(1000 * 1000), uint64_t(0));
}
