// Periodic-error correction (PEC) table.
//
// A lead screw, its nut and the motor coupling produce an angular error that
// repeats once per screw revolution. Because that error is a function of the
// screw's angle - i.e. of the motor position modulo one revolution - it can be
// measured once (bench laser-dot test or guide camera, see docs/) and then
// fed forward: the scheduler adds e(position) to the angle at which each step
// is taken, so the board's TRUE angle follows the sky.
//
// The table holds the measured error (arcsec, true minus geometric angle) at
// N equally spaced screw phases, interpolated linearly and wrapped.
#pragma once

#include <cstdint>

namespace st {

class PecTable {
 public:
  static constexpr int kSize = 64;

  PecTable() { clear(); }

  void clear();
  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }

  bool set(int index, float arcsec);
  float get(int index) const { return (index >= 0 && index < kSize) ? table_[index] : 0.0f; }

  // Error (radians) at a microstep position (relative to home), given the
  // number of microsteps per screw revolution. Returns 0 when disabled.
  double errorRadAt(double steps, double steps_per_rev) const;

  // Largest |slope| of the table in arcsec per table cell; used to check the
  // corrected schedule stays monotonic.
  float maxStepArcsec() const;

 private:
  float table_[kSize];
  bool enabled_ = false;
};

}  // namespace st
