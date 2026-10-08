#include "pec.h"

#include <cmath>

#include "st_math.h"

namespace st {

void PecTable::clear() {
  for (float& v : table_) v = 0.0f;
}

bool PecTable::set(int index, float arcsec) {
  if (index < 0 || index >= kSize) return false;
  if (!std::isfinite(arcsec) || std::fabs(arcsec) > 600.0f) return false;
  table_[index] = arcsec;
  return true;
}

double PecTable::errorRadAt(double steps, double steps_per_rev) const {
  if (!enabled_ || steps_per_rev <= 0.0) return 0.0;
  double phase = std::fmod(steps / steps_per_rev, 1.0);
  if (phase < 0.0) phase += 1.0;
  const double x = phase * kSize;
  int i0 = int(std::floor(x));
  const double frac = x - i0;
  i0 %= kSize;
  const int i1 = (i0 + 1) % kSize;
  const double arcsec = table_[i0] * (1.0 - frac) + table_[i1] * frac;
  return arcsec2rad(arcsec);
}

float PecTable::maxStepArcsec() const {
  float m = 0.0f;
  for (int i = 0; i < kSize; ++i) {
    const float d = std::fabs(table_[(i + 1) % kSize] - table_[i]);
    if (d > m) m = d;
  }
  return m;
}

}  // namespace st
