// Motion controller: turns commands (track, stop, move, home, guide) into a
// time-ordered stream of microstep events.
//
// Design:
//   * The controller is a pure planner. It never touches hardware. It hands
//     out StepEvent{time, direction}; the firmware queues a few milliseconds of
//     them and a timer ISR executes them. On the host, tests "execute" them in
//     simulation.
//   * Tracking is position-based, not rate-based: the k-th step is scheduled
//     at the instant the ideal board angle theta0 + w (t - t0) crosses the
//     half-way point to the next microstep (with optional PEC). Errors never
//     accumulate, and the tangent error of the barn door is removed exactly.
//   * Moves (slew, rewind, jog, homing) use acceleration-limited trapezoids.
//   * Homing: fast approach to the switch, back off, slow approach (exact
//     stop), then move to a small clearance above the switch.
#pragma once

#include <cstdint>

#include "geometry.h"
#include "pec.h"
#include "trapezoid.h"

namespace st {

struct StepEvent {
  int64_t t_us;  // when to pulse STEP (microseconds, same clock as `now_us`)
  int8_t dir;    // +1 = boards open (tracking direction), -1 = close
};

struct MotionLimits {
  double max_speed = 8000.0;         // microsteps/s for slews (5 mm/s on Tr8x2 @ 1/16)
  double accel = 16000.0;            // microsteps/s^2
  double start_speed = 400.0;        // speeds below this may start/stop instantly
  double home_fast_speed = 3200.0;   // microsteps/s
  double home_slow_speed = 320.0;    // <= start_speed, so the final stop is exact
  int32_t home_backoff_steps = 1600; // 1 mm
  int32_t home_clearance_steps = 800;// park 0.5 mm above the switch
  int64_t min_step_interval_us = 100;// ISR limit: 10 kHz
  int32_t max_jog_steps = 32000;     // 20 mm per unhomed jog
};

enum class Mode : uint8_t { Idle = 0, Tracking, Moving, Homing };

enum class Fault : uint8_t {
  None = 0,
  NotHomed,       // command needs a known position
  EndOfTravel,    // tracking reached the soft limit; rewind to continue
  HomeNotFound,   // homing travelled its full distance without the switch
  SwitchStuck,    // switch still pressed after backing off
  LimitHit,       // switch pressed during a normal move (lost steps?)
  BadArgument,
};

const char* modeName(Mode m);
const char* faultName(Fault f);

class MotionController {
 public:
  MotionController() = default;
  MotionController(const BarnDoorGeometry& g, const MotionLimits& l) { configure(g, l); }

  // Replace geometry/limits. Only allowed while idle (returns false otherwise).
  bool configure(const BarnDoorGeometry& g, const MotionLimits& l);

  // ---- commands; `now_us` is the current time ------------------------------
  bool startTracking(double rate_rad_s, int64_t now_us);  // also changes rate
  void stop(int64_t now_us);                               // decelerates moves
  bool moveTo(int32_t target_steps, int64_t now_us);       // needs homing
  bool moveBy(int32_t delta_steps, int64_t now_us);        // jog
  bool gotoAngle(double theta_rad, int64_t now_us);
  bool rewind(int64_t now_us);                             // to the park position
  bool home(int64_t now_us);
  // Declare the current position to be the switch position (bench use without
  // a switch). Only while idle.
  void setHomeHere(int32_t executed_raw);
  // Pulse guide / dither: multiply the tracking rate by `multiplier` (0..2)
  // for `duration_us`.
  bool guide(double multiplier, int64_t duration_us, int64_t now_us);
  void setRateTrimPpm(double ppm, int64_t now_us);
  double rateTrimPpm() const { return trim_ppm_; }
  PecTable& pec() { return pec_; }
  const PecTable& pec() const { return pec_; }

  // ---- inputs --------------------------------------------------------------
  // Feed the home switch level often (every loop). `executed_raw` is the
  // executor's step counter at the time the level was read.
  void updateSwitch(bool pressed, int64_t now_us, int32_t executed_raw);

  // ---- output ----------------------------------------------------------------
  // Time of the next step event, or kNever when nothing is pending.
  int64_t peekTime();
  // Pops the next event (call only when peekTime() != kNever).
  bool next(StepEvent* ev);
  static constexpr int64_t kNever = INT64_MAX;

  // ---- state -----------------------------------------------------------------
  Mode mode() const { return mode_; }
  Fault fault() const { return fault_; }
  void clearFault() { fault_ = Fault::None; }
  bool homed() const { return homed_; }
  bool guiding() const { return guiding_; }
  int32_t rawPosition() const { return raw_; }          // planned, raw counter
  int32_t position() const { return raw_ - zero_; }      // planned, from home
  int32_t zeroRaw() const { return zero_; }
  int32_t parkSteps() const { return limits_.home_clearance_steps; }
  double angleRad() const { return geom_.angleForSteps(position()); }
  double heightMm() const { return geom_.heightForSteps(position()); }
  double trackingRate() const { return base_omega_; }   // incl. trim, excl. guide
  // Seconds of tracking left before the soft limit at the current rate.
  double trackingRemainingSec() const;
  const BarnDoorGeometry& geometry() const { return geom_; }
  const MotionLimits& limits() const { return limits_; }

 private:
  enum class HomePhase : uint8_t { None, Fast, FastStopping, Backoff, Slow, Clearance };

  void invalidate() { have_pending_ = false; }
  bool computePending();
  void finishMove();
  int64_t startTimeFrom(int64_t now_us) const;
  double currentMoveSpeed() const;
  // Plans a move of `delta` steps (signed) with peak speed `vmax`.
  bool beginMove(int32_t delta, double vmax, int64_t now_us);
  void beginStop();
  double trueAngleAt(double steps) const;
  void anchorTracking(int64_t t_us, double theta, double omega);

  BarnDoorGeometry geom_{};
  MotionLimits limits_{};
  PecTable pec_{};

  Mode mode_ = Mode::Idle;
  Fault fault_ = Fault::None;
  bool homed_ = false;
  bool switch_pressed_ = false;
  HomePhase home_phase_ = HomePhase::None;

  int32_t raw_ = 0;
  int32_t zero_ = 0;
  int64_t last_t_ = INT64_MIN / 2;

  // Move state
  TrapezoidMove move_{};
  int64_t move_start_us_ = 0;
  int32_t move_done_ = 0;
  int8_t move_dir_ = 1;
  bool has_followup_ = false;
  bool limit_stop_ = false;  // a limit stop is already in progress
  int32_t followup_target_ = 0;

  // Tracking state
  int64_t anchor_t_us_ = 0;
  double anchor_theta_ = 0.0;
  double omega_ = 0.0;       // current (incl. guide multiplier)
  double base_omega_ = 0.0;  // incl. trim
  double requested_rate_ = 0.0;
  double trim_ppm_ = 0.0;
  bool guiding_ = false;
  int64_t guide_end_us_ = 0;

  bool have_pending_ = false;
  StepEvent pending_{0, 1};
};

}  // namespace st
