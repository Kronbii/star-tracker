#include <cmath>

#include "../star_tracker/src/core/sky.h"
#include "../star_tracker/src/core/st_math.h"
#include "minitest.h"

using namespace st;

namespace {
double clockDiff(double a, double b) {
  double d = std::fmod(a - b, 12.0);
  if (d > 6.0) d -= 12.0;
  if (d < -6.0) d += 12.0;
  return d;
}
}  // namespace

TEST(julian_date_of_unix_epoch_and_j2000) {
  CHECK_NEAR(julianDateFromUnixMs(0), 2440587.5, 1e-9);
  // 2000-01-01T12:00:00Z
  CHECK_NEAR(julianDateFromUnixMs(946728000000LL), 2451545.0, 1e-9);
}

TEST(gmst_matches_meeus_example) {
  // Meeus, Astronomical Algorithms, ex. 12.a: 1987 Apr 10, 0h UT
  // GMST = 13h10m46.3668s
  const double h = rad2deg(gmstRad(2446895.5)) / 15.0;
  CHECK_NEAR(h, 13.0 + 10.0 / 60.0 + 46.3668 / 3600.0, 2e-6);
}

TEST(gmst_matches_skyfield) {
  // Reference values computed with skyfield 1.55 (see tests/test_sky_reference.py)
  struct {
    int64_t ms;
    double gmst_h;
  } cases[] = {{1794081600000LL, 23.135550}, {1799519400000LL, 1.771164},
               {1791504000000LL, 1.175209}};
  for (const auto& c : cases) {
    const double h = rad2deg(gmstRad(julianDateFromUnixMs(c.ms))) / 15.0;
    CHECK_NEAR(h, c.gmst_h, 3e-5);  // ~0.1 s; UTC is used for UT1
  }
}

TEST(precession_matches_meeus_example) {
  // Meeus ex. 21.b: theta Persei, J2000 -> 2028 Nov 13.19 TD. The inputs are
  // the J2000 place with 28.867 years of proper motion already applied.
  const double ra0 = deg2rad(41.054063);
  const double dec0 = deg2rad(49.227750);
  double ra, dec;
  precessFromJ2000(ra0, dec0, 2462088.69, &ra, &dec);
  CHECK_NEAR(rad2deg(ra), 41.547214, 2e-5);
  CHECK_NEAR(rad2deg(dec), 49.348483, 2e-5);
}

TEST(polaris_aid_matches_skyfield) {
  // Beirut, longitude 35.5018 E. skyfield gives the apparent place; we compute
  // the mean place, so allow ~0.03 clock-hours (~1 degree of clock angle).
  struct {
    int64_t ms;
    double sep_deg;
    double clock;
  } cases[] = {{1794081600000LL, 0.62301, 0.8240},
               {1799519400000LL, 0.61738, 11.5015},
               {1791504000000LL, 0.62580, 11.8000}};
  for (const auto& c : cases) {
    const PolarisAid a = polarisAid(c.ms, 35.5018);
    CHECK_NEAR(a.separation_deg, c.sep_deg, 0.01);
    CHECK_NEAR(clockDiff(a.clock_hours, c.clock), 0.0, 0.03);
    CHECK(a.clock_hours > 0.0 && a.clock_hours <= 12.0);
  }
}

TEST(polaris_clock_turns_anticlockwise) {
  // One sidereal hour later Polaris has moved back half a clock-hour.
  const int64_t t0 = 1794081600000LL;
  const PolarisAid a = polarisAid(t0, 35.5);
  const PolarisAid b = polarisAid(t0 + int64_t(kSiderealDaySec / 24.0 * 1000.0), 35.5);
  CHECK_NEAR(clockDiff(b.clock_hours, a.clock_hours), -0.5, 1e-3);
}
