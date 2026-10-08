#include <cmath>

#include "../star_tracker/src/core/trapezoid.h"
#include "minitest.h"

using namespace st;

namespace {

// Checks speeds/accelerations implied by consecutive step times.
void checkProfile(const TrapezoidMove& m, double vmax, double accel) {
  double prev_t = 0.0;
  double prev_v = -1.0;
  double prev_mid = 0.0;
  for (int32_t k = 1; k <= m.distance(); ++k) {
    const double t = m.timeOfStep(k);
    CHECK(t > prev_t || k == 1);
    if (k > 1) {
      const double dt = t - prev_t;
      const double v = 1.0 / dt;
      CHECK(v <= vmax * 1.001);
      const double mid = 0.5 * (t + prev_t);
      if (prev_v > 0) {
        const double a = std::fabs(v - prev_v) / (mid - prev_mid);
        // Discrete estimate: allow some slack near the ends of the ramps.
        CHECK(a <= accel * 1.6);
      }
      prev_v = v;
      prev_mid = mid;
    }
    prev_t = t;
  }
  CHECK_NEAR(m.timeOfStep(m.distance()), m.duration(), 1e-9);
}

}  // namespace

TEST(trapezoid_full_profile) {
  TrapezoidMove m;
  CHECK(m.plan(20000, 0.0, 8000.0, 16000.0));
  CHECK_NEAR(m.peakSpeed(), 8000.0, 1e-9);
  // accel 0.5 s (2000 steps), decel 0.5 s (2000 steps), cruise 16000 steps = 2 s
  CHECK_NEAR(m.duration(), 3.0, 1e-9);
  CHECK_NEAR(m.speedAt(0.25), 4000.0, 1e-9);
  CHECK_NEAR(m.speedAt(1.5), 8000.0, 1e-9);
  CHECK_NEAR(m.speedAt(2.75), 4000.0, 1e-9);
  checkProfile(m, 8000.0, 16000.0);
}

TEST(trapezoid_triangle_when_short) {
  TrapezoidMove m;
  CHECK(m.plan(1000, 0.0, 8000.0, 16000.0));
  CHECK_NEAR(m.peakSpeed(), std::sqrt(16000.0 * 1000.0), 1e-6);
  CHECK(m.peakSpeed() < 8000.0);
  checkProfile(m, 8000.0, 16000.0);
}

TEST(trapezoid_with_initial_speed) {
  TrapezoidMove m;
  CHECK(m.plan(5000, 3000.0, 8000.0, 16000.0));
  CHECK_NEAR(m.speedAt(0.0), 3000.0, 1e-9);
  // The first step comes ~1/v0 after the start.
  CHECK_NEAR(m.timeOfStep(1), 1.0 / 3000.0, 1e-5);
  checkProfile(m, 8000.0, 16000.0);
}

TEST(trapezoid_stopping_distance_and_hard_stop) {
  CHECK_EQ(TrapezoidMove::stoppingDistance(8000.0, 16000.0), 2000);
  CHECK_EQ(TrapezoidMove::stoppingDistance(0.0, 16000.0), 0);
  TrapezoidMove m;
  // 100 steps from 8000 steps/s: decelerates harder than requested.
  CHECK(m.plan(100, 8000.0, 8000.0, 16000.0));
  CHECK_NEAR(m.duration(), 2.0 * 100.0 / 8000.0, 1e-9);
  CHECK_NEAR(m.speedAt(m.duration()), 0.0, 1e-6);
}

TEST(trapezoid_rejects_bad_input) {
  TrapezoidMove m;
  CHECK(!m.plan(0, 0.0, 100.0, 100.0));
  CHECK(!m.plan(10, 0.0, 0.0, 100.0));
  CHECK(!m.plan(10, 0.0, 100.0, 0.0));
}
