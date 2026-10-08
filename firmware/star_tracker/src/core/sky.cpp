#include "sky.h"

#include <cmath>

#include "st_math.h"

namespace st {

namespace {

double wrapTwoPi(double a) {
  a = std::fmod(a, kTwoPi);
  return a < 0.0 ? a + kTwoPi : a;
}

// Polaris (alpha UMi), ICRS J2000.0, with proper motion (SIMBAD).
constexpr double kPolarisRaDeg = (2.0 + 31.0 / 60.0 + 49.09 / 3600.0) * 15.0;
constexpr double kPolarisDecDeg = 89.0 + 15.0 / 60.0 + 50.8 / 3600.0;
constexpr double kPolarisPmRaMasYr = 44.48;   // mu_alpha * cos(dec)
constexpr double kPolarisPmDecMasYr = -11.85;

}  // namespace

double julianDateFromUnixMs(int64_t unix_ms) {
  return 2440587.5 + double(unix_ms) / 86400000.0;
}

double gmstRad(double jd_ut) {
  const double d = jd_ut - 2451545.0;
  const double t = d / 36525.0;
  const double deg = 280.46061837 + 360.98564736629 * d + 0.000387933 * t * t -
                     t * t * t / 38710000.0;
  return wrapTwoPi(deg2rad(deg));
}

double lstRad(double jd_ut, double lon_east_rad) {
  return wrapTwoPi(gmstRad(jd_ut) + lon_east_rad);
}

void precessFromJ2000(double ra0, double dec0, double jd, double* ra, double* dec) {
  const double t = (jd - 2451545.0) / 36525.0;
  const double zeta = arcsec2rad((2306.2181 + (0.30188 + 0.017998 * t) * t) * t);
  const double z = arcsec2rad((2306.2181 + (1.09468 + 0.018203 * t) * t) * t);
  const double theta = arcsec2rad((2004.3109 - (0.42665 + 0.041833 * t) * t) * t);
  const double a = std::cos(dec0) * std::sin(ra0 + zeta);
  const double b = std::cos(theta) * std::cos(dec0) * std::cos(ra0 + zeta) -
                   std::sin(theta) * std::sin(dec0);
  const double c = std::sin(theta) * std::cos(dec0) * std::cos(ra0 + zeta) +
                   std::cos(theta) * std::sin(dec0);
  *ra = wrapTwoPi(std::atan2(a, b) + z);
  *dec = std::atan2(c, std::sqrt(a * a + b * b));  // robust near the pole
}

PolarisAid polarisAid(int64_t unix_ms, double lon_east_deg) {
  PolarisAid out;
  const double jd = julianDateFromUnixMs(unix_ms);
  const double years = (jd - 2451545.0) / 365.25;
  const double dec_j2000 = deg2rad(kPolarisDecDeg);
  const double ra_pm = deg2rad(kPolarisRaDeg) +
                       arcsec2rad(kPolarisPmRaMasYr * 1e-3 * years) / std::cos(dec_j2000);
  const double dec_pm = dec_j2000 + arcsec2rad(kPolarisPmDecMasYr * 1e-3 * years);
  precessFromJ2000(ra_pm, dec_pm, jd, &out.ra_rad, &out.dec_rad);
  out.separation_deg = 90.0 - rad2deg(out.dec_rad);
  out.hour_angle_rad = wrapTwoPi(lstRad(jd, deg2rad(lon_east_deg)) - out.ra_rad);
  // Facing north the sky turns anticlockwise; Polaris is straight above the
  // pole at hour angle 0 and moves anticlockwise by 15 degrees per hour, i.e.
  // half a clock-hour per hour of hour angle.
  const double ha_hours = out.hour_angle_rad * 12.0 / kPi;
  double clock = std::fmod(12.0 - ha_hours / 2.0, 12.0);
  if (clock <= 0.0) clock += 12.0;
  out.clock_hours = clock;
  return out;
}

}  // namespace st
