// Acceleration-limited (trapezoidal) move profile, in microsteps.
//
// A move starts at speed v0 >= 0 (so a stop can be planned while already
// moving), accelerates at `accel` to at most `vmax`, cruises, and decelerates
// to rest after exactly `distance` steps. Step k (1..distance) is taken when
// the continuous position x(t) reaches k.
//
// If the distance is too short to stop from v0 at `accel`, the profile
// decelerates harder (the shortest stop that fits); callers avoid this by
// asking for at least stoppingDistance(v0, accel) steps.
#pragma once

#include <cstdint>

namespace st {

class TrapezoidMove {
 public:
  // Returns false (and plans nothing) for distance < 1, accel <= 0, vmax <= 0.
  bool plan(int32_t distance, double v0, double vmax, double accel);

  int32_t distance() const { return distance_; }
  double duration() const { return t_end_; }
  double peakSpeed() const { return vp_; }

  // Seconds after the start at which step k (1..distance) is taken.
  double timeOfStep(int32_t k) const;
  // Speed (steps/s) at time t after the start.
  double speedAt(double t) const;

  static int32_t stoppingDistance(double v, double accel);

 private:
  int32_t distance_ = 0;
  double v0_ = 0, vp_ = 0, a_ = 1, ad_ = 1;
  double x1_ = 0, xc_ = 0;     // end of accel phase, length of cruise
  double t1_ = 0, t2_ = 0, t_end_ = 0;
};

}  // namespace st
