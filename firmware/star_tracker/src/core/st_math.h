// Hardware-independent constants and tracking rates.
//
// Everything in src/core/ is plain C++17 with no Arduino or ESP-IDF
// dependency, so it compiles both for the ESP32 and on the host, where the
// unit tests in firmware/test/ exercise it.
#pragma once

#include <cstdint>

namespace st {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDegPerRad = 57.295779513082320876;
constexpr double kArcsecPerRad = 206264.80624709636;

constexpr double deg2rad(double d) { return d / kDegPerRad; }
constexpr double rad2deg(double r) { return r * kDegPerRad; }
constexpr double arcsec2rad(double a) { return a / kArcsecPerRad; }
constexpr double rad2arcsec(double r) { return r * kArcsecPerRad; }

// Mean sidereal day in SI seconds (Earth's rotation relative to the stars).
constexpr double kSiderealDaySec = 86164.0905;
// Mean solar day.
constexpr double kSolarDaySec = 86400.0;
// Moon's sidereal period (days): its mean eastward drift against the stars.
constexpr double kSiderealMonthDays = 27.321661;

// Tracking rates, as rotation of the mount about the polar axis (rad/s).
//   sidereal: 15.0411 arcsec/s - stars
//   solar:    15.0000 arcsec/s - the Sun (mean)
//   lunar:    14.4921 arcsec/s - the Moon (mean; the true rate varies ~±1.5%)
constexpr double kSiderealRate = kTwoPi / kSiderealDaySec;
constexpr double kSolarRate = kTwoPi / kSolarDaySec;
constexpr double kLunarRate =
    kSiderealRate - kTwoPi / (kSiderealMonthDays * kSolarDaySec);

enum class Rate : uint8_t { Sidereal = 0, Lunar = 1, Solar = 2, Custom = 3 };

// Rate in rad/s. `custom_arcsec_per_s` is used only for Rate::Custom.
double rateRadPerSec(Rate r, double custom_arcsec_per_s = 0.0);
const char* rateName(Rate r);
// Parses "sidereal", "lunar", "solar" (case-insensitive). Returns false otherwise.
bool parseRateName(const char* s, Rate* out);

}  // namespace st
