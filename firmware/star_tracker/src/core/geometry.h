// Barn-door (tangent-arm, "type 1") geometry with a roller contact.
//
//   * The hinge axis is the polar axis. Put it at the origin, the bottom
//     board along +x.
//   * The lead screw is perpendicular to the bottom board at distance R from
//     the hinge axis. A carriage (nut) travels along it; its roller centre is
//     at (R, h).
//   * The top board's strike plate is a plane through the hinge axis, rotated
//     by the board angle theta. The roller (radius r) touches it from below;
//     if the plane is offset by d from the hinge axis the two add: c = r + d.
//
// Tangency of the roller with the plane gives
//
//     R sin(theta) - h cos(theta) = c
//
//     h(theta) = R tan(theta) - c sec(theta)
//     theta(h) = atan2(h, R) + asin(c / sqrt(R^2 + h^2))
//
// so a constant screw speed does NOT give a constant angular rate (the classic
// barn-door "tangent error"). The firmware never assumes a constant rate: it
// schedules every microstep at the time the ideal angle crosses it, using the
// closed-form inverse above.
#pragma once

#include <cstdint>

namespace st {

struct BarnDoorGeometry {
  double radius_mm = 200.0;         // R: hinge axis to lead-screw axis
  double contact_offset_mm = 11.0;  // c: roller radius + strike-plane offset
  double lead_mm = 2.0;             // lead screw advance per revolution (Tr8x2)
  int32_t full_steps_per_rev = 200; // 1.8 degree stepper
  int32_t microsteps = 16;          // driver microstepping
  double home_height_mm = 3.0;      // roller-centre height h at the home switch
  double max_height_mm = 150.0;     // soft limit (end of useful travel)

  double stepsPerRev() const { return double(full_steps_per_rev) * microsteps; }
  double stepsPerMm() const { return stepsPerRev() / lead_mm; }

  double heightForAngle(double theta_rad) const;
  double angleForHeight(double h_mm) const;

  // Positions are microsteps counted from the home switch (0 = home).
  double heightForSteps(double steps) const { return home_height_mm + steps / stepsPerMm(); }
  double stepsForHeight(double h_mm) const { return (h_mm - home_height_mm) * stepsPerMm(); }
  double angleForSteps(double steps) const { return angleForHeight(heightForSteps(steps)); }
  double stepsForAngle(double theta_rad) const { return stepsForHeight(heightForAngle(theta_rad)); }

  int32_t maxSteps() const;

  // d(theta)/d(step) at a position, rad per microstep (the local "gear ratio").
  double radPerStepAt(double steps) const;

  // Basic sanity: positive sizes, c < R, travel above home, etc.
  bool valid() const;
};

}  // namespace st
