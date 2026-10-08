#include "st_math.h"

#include "text.h"

namespace st {

double rateRadPerSec(Rate r, double custom_arcsec_per_s) {
  switch (r) {
    case Rate::Sidereal:
      return kSiderealRate;
    case Rate::Lunar:
      return kLunarRate;
    case Rate::Solar:
      return kSolarRate;
    case Rate::Custom:
      return arcsec2rad(custom_arcsec_per_s);
  }
  return kSiderealRate;
}

const char* rateName(Rate r) {
  switch (r) {
    case Rate::Sidereal:
      return "sidereal";
    case Rate::Lunar:
      return "lunar";
    case Rate::Solar:
      return "solar";
    case Rate::Custom:
      return "custom";
  }
  return "?";
}

bool parseRateName(const char* s, Rate* out) {
  if (equalsIgnoreCase(s, "sidereal") || equalsIgnoreCase(s, "star")) {
    *out = Rate::Sidereal;
  } else if (equalsIgnoreCase(s, "lunar") || equalsIgnoreCase(s, "moon")) {
    *out = Rate::Lunar;
  } else if (equalsIgnoreCase(s, "solar") || equalsIgnoreCase(s, "sun")) {
    *out = Rate::Solar;
  } else {
    return false;
  }
  return true;
}

}  // namespace st
