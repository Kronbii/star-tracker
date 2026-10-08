#include "trapezoid.h"

#include <cmath>

namespace st {

bool TrapezoidMove::plan(int32_t distance, double v0, double vmax, double accel) {
  if (distance < 1 || !(accel > 0.0) || !(vmax > 0.0)) return false;
  if (v0 < 0.0) v0 = 0.0;
  if (vmax < v0) vmax = v0;
  const double D = double(distance);
  distance_ = distance;
  v0_ = v0;
  a_ = accel;
  ad_ = accel;

  const double x_stop = v0 * v0 / (2.0 * accel);
  if (D <= x_stop) {
    // Too close to stop at `accel`: decelerate only, a little harder.
    vp_ = v0;
    x1_ = 0.0;
    xc_ = 0.0;
    ad_ = v0 * v0 / (2.0 * D);
  } else {
    const double x_acc = (vmax * vmax - v0 * v0) / (2.0 * accel);
    const double x_dec = vmax * vmax / (2.0 * accel);
    if (x_acc + x_dec <= D) {
      vp_ = vmax;
      x1_ = x_acc;
      xc_ = D - x_acc - x_dec;
    } else {
      // Triangle: never reaches vmax.
      vp_ = std::sqrt(accel * D + 0.5 * v0 * v0);
      x1_ = (vp_ * vp_ - v0 * v0) / (2.0 * accel);
      xc_ = 0.0;
    }
  }
  t1_ = (vp_ - v0_) / a_;
  t2_ = t1_ + (vp_ > 0.0 ? xc_ / vp_ : 0.0);
  t_end_ = t2_ + vp_ / ad_;
  return true;
}

double TrapezoidMove::timeOfStep(int32_t k) const {
  const double x = double(k);
  if (x <= x1_) {
    // v0 t + a t^2 / 2 = x, written to avoid cancellation.
    return 2.0 * x / (v0_ + std::sqrt(v0_ * v0_ + 2.0 * a_ * x));
  }
  if (x <= x1_ + xc_) {
    return t1_ + (x - x1_) / vp_;
  }
  double r = double(distance_) - x;
  if (r < 0.0) r = 0.0;
  return t_end_ - std::sqrt(2.0 * r / ad_);
}

double TrapezoidMove::speedAt(double t) const {
  if (t <= 0.0) return v0_;
  if (t <= t1_) return v0_ + a_ * t;
  if (t <= t2_) return vp_;
  if (t <= t_end_) return vp_ - ad_ * (t - t2_);
  return 0.0;
}

int32_t TrapezoidMove::stoppingDistance(double v, double accel) {
  if (v <= 0.0 || accel <= 0.0) return 0;
  return int32_t(std::ceil(v * v / (2.0 * accel)));
}

}  // namespace st
