// Small sky-geometry helpers for the polar-alignment aid.
//
// The sighting tube's reticle is a ring centred on the true celestial pole
// with Polaris' angular distance as radius. To use it you need Polaris' hour
// angle: the firmware computes it from the phone's clock (sent by the web UI)
// and the site longitude, and shows it as a clock position.
//
// Accuracy: mean place (IAU 1976 precession + proper motion, no nutation or
// aberration) and UTC used for UT1. Errors are tens of arcseconds, i.e. a
// fraction of a degree of clock angle - far below what a sighting tube resolves.
#pragma once

#include <cstdint>

namespace st {

double julianDateFromUnixMs(int64_t unix_ms);
// Greenwich mean sidereal time (radians, 0..2pi), Meeus eq. 12.4.
double gmstRad(double jd_ut);
// Local mean sidereal time; longitude east-positive (radians).
double lstRad(double jd_ut, double lon_east_rad);

// Precess mean equatorial coordinates from J2000.0 to the epoch `jd`
// (IAU 1976, Lieske et al.). Angles in radians.
void precessFromJ2000(double ra0, double dec0, double jd, double* ra, double* dec);

struct PolarisAid {
  double ra_rad = 0;          // mean RA of date
  double dec_rad = 0;         // mean Dec of date
  double separation_deg = 0;  // distance from the pole
  double hour_angle_rad = 0;  // 0..2pi, westward from the meridian
  // Polaris' position around the pole as seen naked-eye facing north,
  // as a 12-hour clock reading (12 = straight up, 3 = right/east, ...).
  double clock_hours = 0;
};

PolarisAid polarisAid(int64_t unix_ms, double lon_east_deg);

}  // namespace st
