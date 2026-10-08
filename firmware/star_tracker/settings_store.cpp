#include "settings_store.h"

#include <Preferences.h>

namespace store {

namespace {
constexpr const char* kNs = "tracker";
constexpr uint32_t kVersion = 1;
}  // namespace

bool load(st::Settings* s) {
  Preferences p;
  if (!p.begin(kNs, true)) return false;
  if (p.getUInt("ver", 0) != kVersion) {
    p.end();
    return false;
  }
  s->geom.radius_mm = p.getDouble("R", s->geom.radius_mm);
  s->geom.contact_offset_mm = p.getDouble("c", s->geom.contact_offset_mm);
  s->geom.lead_mm = p.getDouble("lead", s->geom.lead_mm);
  s->geom.home_height_mm = p.getDouble("home", s->geom.home_height_mm);
  s->geom.max_height_mm = p.getDouble("max", s->geom.max_height_mm);
  s->trim_ppm = p.getDouble("trim", s->trim_ppm);
  s->lon_deg = p.getDouble("lon", s->lon_deg);
  s->pec_on = p.getBool("pec_on", false);
  if (p.getBytesLength("pec") == sizeof(s->pec)) p.getBytes("pec", s->pec, sizeof(s->pec));
  p.end();
  return true;
}

void save(const st::Settings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUInt("ver", kVersion);
  p.putDouble("R", s.geom.radius_mm);
  p.putDouble("c", s.geom.contact_offset_mm);
  p.putDouble("lead", s.geom.lead_mm);
  p.putDouble("home", s.geom.home_height_mm);
  p.putDouble("max", s.geom.max_height_mm);
  p.putDouble("trim", s.trim_ppm);
  p.putDouble("lon", s.lon_deg);
  p.putBool("pec_on", s.pec_on);
  p.putBytes("pec", s.pec, sizeof(s.pec));
  p.end();
}

}  // namespace store
