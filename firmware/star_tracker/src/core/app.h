// TrackerApp: everything the firmware does that is not hardware.
//
// Owns the motion controller and the intervalometer, executes text commands
// (from serial or the web API), keeps settings, and renders status as JSON.
// The ESP32 sketch only adds pins, the timer ISR, Wi-Fi and flash storage, so
// this whole command path is exercised by the host tests.
#pragma once

#include <cstddef>
#include <cstdint>

#include "command.h"
#include "geometry.h"
#include "intervalometer.h"
#include "motion.h"
#include "pec.h"

namespace st {

struct Settings {
  BarnDoorGeometry geom{};
  double trim_ppm = 0.0;
  double lon_deg = 35.50;  // Beirut
  bool pec_on = false;
  float pec[PecTable::kSize] = {};
};

class TrackerApp {
 public:
  TrackerApp() = default;

  void begin(const Settings& s, const MotionLimits& limits = MotionLimits{});

  // Executes one command line and writes a JSON reply into `out`.
  // `executed_raw` is the step executor's counter. Returns true on success.
  bool handle(const char* line, int64_t now_us, int32_t executed_raw, char* out, size_t n);

  // Periodic work (every ~1 ms): camera state machine, safety interlocks.
  void tick(int64_t now_us);

  void statusJson(int64_t now_us, char* out, size_t n) const;

  MotionController& motion() { return motion_; }
  const MotionController& motion() const { return motion_; }
  Intervalometer& camera() { return camera_; }
  const Settings& settings() const { return settings_; }

  // Persistence handshake: the sketch saves settings() when this is true.
  bool takeSaveRequest() {
    const bool r = save_requested_;
    save_requested_ = false;
    return r;
  }

  void setUnixTime(int64_t unix_ms, int64_t now_us);
  bool timeValid() const { return time_valid_; }
  int64_t unixMs(int64_t now_us) const { return unix_at_boot_ms_ + now_us / 1000; }

  void setBatteryVolts(double v) { battery_v_ = v; }
  bool shotStoppedByTravel() const { return shot_stopped_by_travel_; }

 private:
  bool reply(char* out, size_t n, bool ok, const char* error = nullptr);
  bool applySet(const Command& c, char* out, size_t n);

  Settings settings_{};
  MotionLimits limits_{};
  MotionController motion_{};
  Intervalometer camera_{};
  Rate rate_ = Rate::Sidereal;
  bool save_requested_ = false;
  bool time_valid_ = false;
  int64_t unix_at_boot_ms_ = 0;
  double battery_v_ = -1.0;
  bool shot_stopped_by_travel_ = false;
  Mode last_mode_ = Mode::Idle;
};

const char* helpText();

}  // namespace st
