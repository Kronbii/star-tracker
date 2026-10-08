#include "geometry.h"

#include <cmath>

namespace st {

double BarnDoorGeometry::heightForAngle(double theta_rad) const {
  const double c = std::cos(theta_rad);
  return (radius_mm * std::sin(theta_rad) - contact_offset_mm) / c;
}

double BarnDoorGeometry::angleForHeight(double h_mm) const {
  const double rho = std::sqrt(radius_mm * radius_mm + h_mm * h_mm);
  return std::atan2(h_mm, radius_mm) + std::asin(contact_offset_mm / rho);
}

int32_t BarnDoorGeometry::maxSteps() const {
  return int32_t(std::floor(stepsForHeight(max_height_mm)));
}

double BarnDoorGeometry::radPerStepAt(double steps) const {
  // Central difference over one microstep; the function is smooth.
  return angleForSteps(steps + 0.5) - angleForSteps(steps - 0.5);
}

bool BarnDoorGeometry::valid() const {
  return radius_mm > 0.0 && lead_mm > 0.0 && full_steps_per_rev > 0 && microsteps > 0 &&
         contact_offset_mm >= 0.0 && contact_offset_mm < radius_mm &&
         max_height_mm > home_height_mm;
}

}  // namespace st
