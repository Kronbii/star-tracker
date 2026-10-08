#include <cmath>

#include "../star_tracker/src/core/geometry.h"
#include "../star_tracker/src/core/st_math.h"
#include "minitest.h"

using namespace st;

TEST(rates_match_known_values) {
  CHECK_NEAR(rad2arcsec(kSiderealRate), 15.041067, 1e-5);
  CHECK_NEAR(rad2arcsec(kSolarRate), 15.0, 1e-9);
  CHECK_NEAR(rad2arcsec(kLunarRate), 14.4921, 1e-3);
  CHECK_NEAR(rateRadPerSec(Rate::Custom, 15.0), kSolarRate, 1e-15);
  Rate r;
  CHECK(parseRateName("Lunar", &r) && r == Rate::Lunar);
  CHECK(parseRateName("SIDEREAL", &r) && r == Rate::Sidereal);
  CHECK(!parseRateName("king", &r));
}

TEST(geometry_round_trip) {
  BarnDoorGeometry g;
  for (double h = -5.0; h <= 200.0; h += 0.37) {
    const double th = g.angleForHeight(h);
    CHECK_NEAR(g.heightForAngle(th), h, 1e-9);
    // Tangency condition R sin(theta) - h cos(theta) = c.
    CHECK_NEAR(g.radius_mm * std::sin(th) - h * std::cos(th), g.contact_offset_mm, 1e-9);
  }
}

TEST(geometry_reduces_to_tangent_without_roller) {
  BarnDoorGeometry g;
  g.contact_offset_mm = 0.0;
  for (double deg = 0.0; deg < 45.0; deg += 1.0) {
    const double th = deg2rad(deg);
    CHECK_NEAR(g.heightForAngle(th), g.radius_mm * std::tan(th), 1e-9);
  }
}

TEST(geometry_local_ratio_matches_analytic_derivative) {
  // d(theta)/dh = cos^2(theta) / (R - c sin(theta))
  BarnDoorGeometry g;
  for (double s = 0.0; s < g.maxSteps(); s += 10007.0) {
    const double th = g.angleForSteps(s);
    const double analytic =
        std::cos(th) * std::cos(th) / (g.radius_mm - g.contact_offset_mm * std::sin(th));
    CHECK_NEAR(g.radPerStepAt(s) * g.stepsPerMm(), analytic, analytic * 1e-6);
  }
}

TEST(geometry_default_numbers) {
  BarnDoorGeometry g;
  CHECK_NEAR(g.stepsPerMm(), 1600.0, 1e-12);
  CHECK_EQ(g.maxSteps(), int32_t((150.0 - 3.0) * 1600.0));
  CHECK(g.valid());
  // One microstep near home is about 0.64 arcsec on the sky.
  const double as = rad2arcsec(g.radPerStepAt(0));
  CHECK(as > 0.6 && as < 0.66);
  // Home angle ~ 4 degrees, full travel ~ 37 degrees (~2.4 h of sidereal).
  CHECK_NEAR(rad2deg(g.angleForSteps(0)), 4.0, 0.1);
  const double span = g.angleForSteps(g.maxSteps()) - g.angleForSteps(0);
  CHECK(span / kSiderealRate > 2.2 * 3600 && span / kSiderealRate < 2.6 * 3600);
}

TEST(geometry_validation) {
  BarnDoorGeometry g;
  g.contact_offset_mm = g.radius_mm;
  CHECK(!g.valid());
  g = BarnDoorGeometry{};
  g.max_height_mm = g.home_height_mm;
  CHECK(!g.valid());
  g = BarnDoorGeometry{};
  g.lead_mm = 0;
  CHECK(!g.valid());
}

TEST(constant_rate_drive_has_large_tangent_error) {
  // Why the firmware corrects: driving the screw at a constant speed matched
  // to the start produces a large error within an hour.
  BarnDoorGeometry g;
  const double th0 = g.angleForSteps(0);
  const double v = 1.0 / (g.radPerStepAt(0) / kSiderealRate);  // steps/s at start
  const double t = 3600.0;
  const double err = g.angleForSteps(v * t) - (th0 + kSiderealRate * t);
  CHECK(std::fabs(rad2arcsec(err)) > 30.0);
}
