#include "app.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "json_writer.h"
#include "sky.h"
#include "st_math.h"

namespace st {

const char* helpText() {
  return "commands: status | track [sidereal|lunar|solar|<arcsec/s>] | stop | rewind | home | "
         "sethome | goto <deg> | jog <mm> | guide <ms> [mult] | trim <ppm> | "
         "shoot <n> <exp_s> [gap_s] [delay_s] | shoot stop | test [exp_s] | time <unix_ms> | "
         "set radius|offset|lead|home|max|lon <v> | pec on|off|clear|<i> <arcsec> | save";
}

void TrackerApp::begin(const Settings& s, const MotionLimits& limits) {
  settings_ = s;
  limits_ = limits;
  if (!settings_.geom.valid()) settings_.geom = BarnDoorGeometry{};
  motion_.configure(settings_.geom, limits_);
  motion_.setRateTrimPpm(settings_.trim_ppm, 0);
  for (int i = 0; i < PecTable::kSize; ++i) motion_.pec().set(i, settings_.pec[i]);
  motion_.pec().setEnabled(settings_.pec_on);
}

void TrackerApp::setUnixTime(int64_t unix_ms, int64_t now_us) {
  unix_at_boot_ms_ = unix_ms - now_us / 1000;
  time_valid_ = true;
}

bool TrackerApp::reply(char* out, size_t n, bool ok, const char* error) {
  JsonWriter w(out, n);
  w.beginObject().fieldBool("ok", ok);
  if (!ok && error) w.field("error", error);
  if (!ok && !error) w.field("error", faultName(motion_.fault()));
  w.endObject();
  return ok;
}

bool TrackerApp::applySet(const Command& c, char* out, size_t n) {
  if (c.key == SetKey::Longitude) {
    settings_.lon_deg = c.a;
    return reply(out, n, true);
  }
  if (motion_.mode() != Mode::Idle) return reply(out, n, false, "stop first");
  BarnDoorGeometry g = settings_.geom;
  switch (c.key) {
    case SetKey::Radius:
      g.radius_mm = c.a;
      break;
    case SetKey::Offset:
      g.contact_offset_mm = c.a;
      break;
    case SetKey::Lead:
      g.lead_mm = c.a;
      break;
    case SetKey::HomeHeight:
      g.home_height_mm = c.a;
      break;
    case SetKey::MaxHeight:
      g.max_height_mm = c.a;
      break;
    default:
      return reply(out, n, false, "bad key");
  }
  if (!g.valid() || !motion_.configure(g, limits_)) return reply(out, n, false, "invalid geometry");
  settings_.geom = g;
  return reply(out, n, true);
}

bool TrackerApp::handle(const char* line, int64_t now_us, int32_t executed_raw, char* out,
                        size_t n) {
  Command c;
  if (!parseCommand(line, &c)) return reply(out, n, false, c.error);
  switch (c.type) {
    case CmdType::Help: {
      JsonWriter w(out, n);
      w.beginObject().fieldBool("ok", true).field("help", helpText()).endObject();
      return true;
    }
    case CmdType::Status:
      statusJson(now_us, out, n);
      return true;
    case CmdType::Track: {
      rate_ = c.rate;
      return reply(out, n, motion_.startTracking(rateRadPerSec(c.rate, c.a), now_us));
    }
    case CmdType::Stop:
      motion_.stop(now_us);
      return reply(out, n, true);
    case CmdType::Rewind:
      return reply(out, n, motion_.rewind(now_us));
    case CmdType::Home:
      return reply(out, n, motion_.home(now_us), "busy");
    case CmdType::SetHome:
      if (motion_.mode() != Mode::Idle) return reply(out, n, false, "stop first");
      motion_.setHomeHere(executed_raw);
      return reply(out, n, true);
    case CmdType::Goto:
      return reply(out, n, motion_.gotoAngle(deg2rad(c.a), now_us));
    case CmdType::Jog:
      return reply(out, n,
                   motion_.moveBy(int32_t(std::lround(c.a * settings_.geom.stepsPerMm())), now_us));
    case CmdType::Guide:
      return reply(out, n, motion_.guide(c.b, int64_t(c.a * 1000.0), now_us), "not tracking");
    case CmdType::Trim:
      settings_.trim_ppm = c.a;
      motion_.setRateTrimPpm(c.a, now_us);
      return reply(out, n, true);
    case CmdType::Shoot: {
      ShootPlan p;
      p.frames = uint16_t(c.a);
      p.exposure_ms = uint32_t(std::lround(c.b * 1000.0));
      p.gap_ms = uint32_t(std::lround(c.c * 1000.0));
      p.delay_ms = uint32_t(std::lround(c.d * 1000.0));
      shot_stopped_by_travel_ = false;
      return reply(out, n, camera_.start(p, now_us), "invalid plan");
    }
    case CmdType::ShootStop:
      camera_.abort();
      return reply(out, n, true);
    case CmdType::TestShot: {
      ShootPlan p;
      p.frames = 1;
      p.exposure_ms = uint32_t(std::lround(c.a * 1000.0));
      p.gap_ms = 0;
      p.delay_ms = 0;
      return reply(out, n, camera_.start(p, now_us), "invalid plan");
    }
    case CmdType::Time:
      setUnixTime(c.i64, now_us);
      return reply(out, n, true);
    case CmdType::Set:
      return applySet(c, out, n);
    case CmdType::PecClear:
      motion_.pec().clear();
      for (float& v : settings_.pec) v = 0.0f;
      return reply(out, n, true);
    case CmdType::PecEnable:
      if (motion_.mode() == Mode::Tracking) return reply(out, n, false, "stop first");
      motion_.pec().setEnabled(c.flag);
      settings_.pec_on = c.flag;
      return reply(out, n, true);
    case CmdType::PecSet: {
      if (motion_.mode() == Mode::Tracking) return reply(out, n, false, "stop first");
      const int i = int(c.a);
      motion_.pec().set(i, float(c.b));
      settings_.pec[i] = float(c.b);
      return reply(out, n, true);
    }
    case CmdType::Save:
      save_requested_ = true;
      return reply(out, n, true);
    case CmdType::Invalid:
      break;
  }
  return reply(out, n, false, "unhandled");
}

void TrackerApp::tick(int64_t now_us) {
  camera_.update(now_us);
  // Interlock: if tracking just ran out of travel mid-sequence, further frames
  // would trail, so abort the sequence.
  const Mode mode = motion_.mode();
  if (last_mode_ == Mode::Tracking && mode == Mode::Idle &&
      motion_.fault() == Fault::EndOfTravel && camera_.active()) {
    camera_.abort();
    shot_stopped_by_travel_ = true;
  }
  last_mode_ = mode;
}

void TrackerApp::statusJson(int64_t now_us, char* out, size_t n) const {
  JsonWriter w(out, n);
  const MotionController& m = motion_;
  const bool tracking = m.mode() == Mode::Tracking;
  w.beginObject();
  w.fieldBool("ok", true);
  w.field("mode", modeName(m.mode()));
  w.field("fault", faultName(m.fault()));
  w.fieldBool("homed", m.homed());
  w.field("rate", rateName(rate_));
  w.field("rate_arcsec_s", tracking ? rad2arcsec(m.trackingRate()) : 0.0, 4);
  w.field("trim_ppm", settings_.trim_ppm, 1);
  w.fieldBool("guiding", m.guiding());
  w.fieldBool("pec", m.pec().enabled());
  w.field("pos_steps", int64_t(m.position()));
  w.field("height_mm", m.heightMm(), 3);
  w.field("angle_deg", rad2deg(m.angleRad()), 4);
  w.field("remaining_s", m.homed() ? m.trackingRemainingSec() : 0.0, 0);
  w.field("uptime_s", double(now_us) * 1e-6, 1);
  if (battery_v_ >= 0.0) w.field("battery_v", battery_v_, 2);

  w.beginObject("camera");
  w.field("state", shotStateName(camera_.state()));
  w.field("done", int64_t(camera_.framesDone()));
  w.field("total", int64_t(camera_.framesTotal()));
  w.field("exposure_s", double(camera_.plan().exposure_ms) * 1e-3, 1);
  w.field("remaining_s", double(camera_.remainingMs(now_us)) * 1e-3, 0);
  w.fieldBool("shutter", camera_.shutterClosed());
  w.endObject();

  w.beginObject("geometry");
  w.field("radius_mm", settings_.geom.radius_mm, 3);
  w.field("offset_mm", settings_.geom.contact_offset_mm, 3);
  w.field("lead_mm", settings_.geom.lead_mm, 4);
  w.field("home_mm", settings_.geom.home_height_mm, 3);
  w.field("max_mm", settings_.geom.max_height_mm, 3);
  w.endObject();

  w.field("lon_deg", settings_.lon_deg, 4);
  w.fieldBool("time_valid", time_valid_);
  if (time_valid_) {
    const int64_t ms = unixMs(now_us);
    const PolarisAid a = polarisAid(ms, settings_.lon_deg);
    w.field("unix_ms", ms);
    w.beginObject("polaris");
    w.field("clock", a.clock_hours, 3);
    w.field("hour_angle_h", a.hour_angle_rad * 12.0 / kPi, 3);
    w.field("separation_deg", a.separation_deg, 3);
    w.endObject();
  }
  w.endObject();
}

}  // namespace st
