#include "motion.h"

#include <cmath>
#include <cstdlib>

namespace st {

const char* modeName(Mode m) {
  switch (m) {
    case Mode::Idle:
      return "idle";
    case Mode::Tracking:
      return "tracking";
    case Mode::Moving:
      return "moving";
    case Mode::Homing:
      return "homing";
  }
  return "?";
}

const char* faultName(Fault f) {
  switch (f) {
    case Fault::None:
      return "none";
    case Fault::NotHomed:
      return "not_homed";
    case Fault::EndOfTravel:
      return "end_of_travel";
    case Fault::HomeNotFound:
      return "home_not_found";
    case Fault::SwitchStuck:
      return "switch_stuck";
    case Fault::LimitHit:
      return "limit_hit";
    case Fault::BadArgument:
      return "bad_argument";
  }
  return "?";
}

bool MotionController::configure(const BarnDoorGeometry& g, const MotionLimits& l) {
  if (mode_ != Mode::Idle || !g.valid()) return false;
  if (!(l.max_speed > 0) || !(l.accel > 0) || l.home_slow_speed > l.start_speed) return false;
  geom_ = g;
  limits_ = l;
  invalidate();
  return true;
}

// ---------------------------------------------------------------------------
// Helpers

int64_t MotionController::startTimeFrom(int64_t now_us) const {
  const int64_t min_t = last_t_ + limits_.min_step_interval_us;
  return now_us > min_t ? now_us : min_t;
}

double MotionController::trueAngleAt(double steps) const {
  return geom_.angleForSteps(steps) + pec_.errorRadAt(steps, geom_.stepsPerRev());
}

void MotionController::anchorTracking(int64_t t_us, double theta, double omega) {
  anchor_t_us_ = t_us;
  anchor_theta_ = theta;
  omega_ = omega;
  invalidate();
}

double MotionController::currentMoveSpeed() const {
  if (mode_ != Mode::Moving && mode_ != Mode::Homing) return 0.0;
  if (move_done_ >= move_.distance()) return 0.0;
  if (move_done_ == 0) return move_.speedAt(0.0);
  return move_.speedAt(double(last_t_ - move_start_us_) * 1e-6);
}

bool MotionController::beginMove(int32_t delta, double vmax, int64_t now_us) {
  if (delta == 0) return false;
  // From rest: start at the start/stop speed (no ramp needed below it).
  const double v0 = limits_.start_speed < vmax ? limits_.start_speed : vmax;
  if (!move_.plan(std::abs(delta), v0, vmax, limits_.accel)) return false;
  move_dir_ = delta > 0 ? 1 : -1;
  move_done_ = 0;
  limit_stop_ = false;
  // Step k fires when x(t) = k; with x(0) = 0 the first step lands ~1/v0
  // after the start time.
  move_start_us_ = startTimeFrom(now_us);
  invalidate();
  return true;
}

void MotionController::beginStop() {
  const double v = currentMoveSpeed();
  if (v <= limits_.start_speed) {
    move_done_ = move_.distance();  // nothing left
    invalidate();
    finishMove();
    return;
  }
  int32_t d = TrapezoidMove::stoppingDistance(v, limits_.accel);
  if (d < 1) d = 1;
  move_.plan(d, v, v, limits_.accel);
  move_start_us_ = last_t_;  // continue from the last emitted step
  move_done_ = 0;
  invalidate();
}

void MotionController::finishMove() {
  invalidate();
  if (mode_ == Mode::Homing) {
    switch (home_phase_) {
      case HomePhase::Fast:
      case HomePhase::Slow:
        fault_ = Fault::HomeNotFound;
        home_phase_ = HomePhase::None;
        mode_ = Mode::Idle;
        return;
      case HomePhase::FastStopping:
        home_phase_ = HomePhase::Backoff;
        beginMove(limits_.home_backoff_steps, limits_.home_fast_speed, last_t_);
        return;
      case HomePhase::Backoff:
        if (switch_pressed_) {
          fault_ = Fault::SwitchStuck;
          home_phase_ = HomePhase::None;
          mode_ = Mode::Idle;
          return;
        }
        home_phase_ = HomePhase::Slow;
        beginMove(-(2 * limits_.home_backoff_steps + limits_.home_clearance_steps),
                  limits_.home_slow_speed, last_t_);
        return;
      case HomePhase::Clearance:
      case HomePhase::None:
        home_phase_ = HomePhase::None;
        mode_ = Mode::Idle;
        return;
    }
  }
  if (has_followup_) {
    has_followup_ = false;
    const int32_t delta = followup_target_ - position();
    if (delta != 0 && beginMove(delta, limits_.max_speed, last_t_)) {
      mode_ = Mode::Moving;
      return;
    }
  }
  mode_ = Mode::Idle;
}

double MotionController::trackingRemainingSec() const {
  if (!(base_omega_ > 0.0)) return 0.0;
  const double rem = geom_.angleForSteps(geom_.maxSteps()) - angleRad();
  return rem > 0.0 ? rem / base_omega_ : 0.0;
}

// ---------------------------------------------------------------------------
// Commands

bool MotionController::startTracking(double rate_rad_s, int64_t now_us) {
  if (!(rate_rad_s > 0.0) || rate_rad_s > 1e-3) {
    fault_ = Fault::BadArgument;
    return false;
  }
  if (!homed_) {
    fault_ = Fault::NotHomed;
    return false;
  }
  if (mode_ == Mode::Moving || mode_ == Mode::Homing) return false;
  if (position() + 1 > geom_.maxSteps()) {
    fault_ = Fault::EndOfTravel;
    return false;
  }
  const double base = rate_rad_s * (1.0 + trim_ppm_ * 1e-6);
  const int64_t t = now_us > last_t_ ? now_us : last_t_;
  double theta;
  if (mode_ == Mode::Tracking) {
    // Rate change: keep the ideal angle continuous.
    if (guiding_ && t > guide_end_us_) {
      theta = anchor_theta_ + omega_ * double(guide_end_us_ - anchor_t_us_) * 1e-6 +
              base_omega_ * double(t - guide_end_us_) * 1e-6;
    } else {
      theta = anchor_theta_ + omega_ * double(t - anchor_t_us_) * 1e-6;
    }
  } else {
    theta = trueAngleAt(position());
  }
  requested_rate_ = rate_rad_s;
  base_omega_ = base;
  guiding_ = false;
  fault_ = Fault::None;
  mode_ = Mode::Tracking;
  anchorTracking(t, theta, base);
  return true;
}

void MotionController::stop(int64_t /*now_us*/) {
  switch (mode_) {
    case Mode::Idle:
      return;
    case Mode::Tracking:
      mode_ = Mode::Idle;
      guiding_ = false;
      invalidate();
      return;
    case Mode::Homing:
      home_phase_ = HomePhase::None;
      mode_ = Mode::Moving;
      has_followup_ = false;
      beginStop();
      return;
    case Mode::Moving:
      has_followup_ = false;
      beginStop();
      return;
  }
}

bool MotionController::moveTo(int32_t target, int64_t now_us) {
  if (!homed_) {
    fault_ = Fault::NotHomed;
    return false;
  }
  if (mode_ == Mode::Homing) return false;
  const int32_t lo = 0;
  const int32_t hi = geom_.maxSteps();
  if (target < lo) target = lo;
  if (target > hi) target = hi;

  if (mode_ == Mode::Tracking) {
    mode_ = Mode::Idle;  // tracking speed is far below the start/stop speed
    guiding_ = false;
  }
  const int32_t delta = target - position();
  if (mode_ == Mode::Moving) {
    const double v = currentMoveSpeed();
    if (v > limits_.start_speed) {
      const int8_t dir = delta > 0 ? 1 : -1;
      if (delta != 0 && dir == move_dir_ &&
          std::abs(delta) >= TrapezoidMove::stoppingDistance(v, limits_.accel)) {
        // Same direction, room to stop: re-plan without stopping.
        move_.plan(std::abs(delta), v, limits_.max_speed, limits_.accel);
        move_start_us_ = last_t_;
        move_done_ = 0;
        invalidate();
        return true;
      }
      // Otherwise stop first, then go.
      has_followup_ = true;
      followup_target_ = target;
      beginStop();
      return true;
    }
    mode_ = Mode::Idle;
  }
  if (delta == 0) {
    invalidate();
    return true;
  }
  if (!beginMove(delta, limits_.max_speed, now_us)) return false;
  mode_ = Mode::Moving;
  return true;
}

bool MotionController::moveBy(int32_t delta, int64_t now_us) {
  if (homed_) return moveTo(position() + delta, now_us);
  if (mode_ != Mode::Idle) return false;
  if (std::abs(delta) > limits_.max_jog_steps) {
    fault_ = Fault::BadArgument;
    return false;
  }
  if (delta == 0) return true;
  if (!beginMove(delta, limits_.max_speed, now_us)) return false;
  mode_ = Mode::Moving;
  return true;
}

bool MotionController::gotoAngle(double theta_rad, int64_t now_us) {
  if (!std::isfinite(theta_rad)) {
    fault_ = Fault::BadArgument;
    return false;
  }
  const double s = geom_.stepsForAngle(theta_rad);
  if (!std::isfinite(s)) {
    fault_ = Fault::BadArgument;
    return false;
  }
  return moveTo(int32_t(std::lround(s)), now_us);
}

bool MotionController::rewind(int64_t now_us) { return moveTo(parkSteps(), now_us); }

bool MotionController::home(int64_t now_us) {
  if (mode_ == Mode::Moving || mode_ == Mode::Homing) return false;
  mode_ = Mode::Homing;
  guiding_ = false;
  has_followup_ = false;
  fault_ = Fault::None;
  homed_ = false;
  if (switch_pressed_) {
    home_phase_ = HomePhase::Backoff;
    beginMove(limits_.home_backoff_steps, limits_.home_fast_speed, now_us);
  } else {
    home_phase_ = HomePhase::Fast;
    const int32_t travel = geom_.maxSteps() + 4 * limits_.home_backoff_steps;
    beginMove(-travel, limits_.home_fast_speed, now_us);
  }
  return true;
}

void MotionController::setHomeHere(int32_t executed_raw) {
  if (mode_ != Mode::Idle) return;
  (void)executed_raw;
  zero_ = raw_;  // idle: everything planned is (or will be) executed
  homed_ = true;
  if (fault_ == Fault::NotHomed) fault_ = Fault::None;
}

bool MotionController::guide(double multiplier, int64_t duration_us, int64_t now_us) {
  if (mode_ != Mode::Tracking) return false;
  if (!(multiplier >= 0.0 && multiplier <= 2.0) || duration_us <= 0) {
    fault_ = Fault::BadArgument;
    return false;
  }
  const int64_t t = now_us > last_t_ ? now_us : last_t_;
  double theta;
  if (guiding_ && t > guide_end_us_) {
    theta = anchor_theta_ + omega_ * double(guide_end_us_ - anchor_t_us_) * 1e-6 +
            base_omega_ * double(t - guide_end_us_) * 1e-6;
  } else {
    theta = anchor_theta_ + omega_ * double(t - anchor_t_us_) * 1e-6;
  }
  guiding_ = true;
  guide_end_us_ = t + duration_us;
  anchorTracking(t, theta, base_omega_ * multiplier);
  return true;
}

void MotionController::setRateTrimPpm(double ppm, int64_t now_us) {
  if (!std::isfinite(ppm) || std::fabs(ppm) > 50000.0) {
    fault_ = Fault::BadArgument;
    return;
  }
  trim_ppm_ = ppm;
  if (mode_ == Mode::Tracking) startTracking(requested_rate_, now_us);
}

// ---------------------------------------------------------------------------
// Inputs

void MotionController::updateSwitch(bool pressed, int64_t now_us, int32_t executed_raw) {
  switch_pressed_ = pressed;
  if (!pressed) return;
  if (mode_ == Mode::Homing) {
    if (home_phase_ == HomePhase::Fast) {
      home_phase_ = HomePhase::FastStopping;
      beginStop();
    } else if (home_phase_ == HomePhase::Slow) {
      // Slow approach runs below the start/stop speed: the stop is exact.
      zero_ = executed_raw;
      homed_ = true;
      home_phase_ = HomePhase::Clearance;
      const int32_t delta = limits_.home_clearance_steps - position();
      if (delta <= 0 || !beginMove(delta, limits_.home_fast_speed, now_us)) {
        home_phase_ = HomePhase::None;
        mode_ = Mode::Idle;
        invalidate();
      }
    }
    return;
  }
  if (mode_ == Mode::Moving && move_dir_ < 0 && !limit_stop_) {
    // Unexpected switch hit while closing: re-reference and stop (once).
    limit_stop_ = true;
    fault_ = Fault::LimitHit;
    zero_ = executed_raw;
    has_followup_ = false;
    beginStop();
  }
}

// ---------------------------------------------------------------------------
// Step generation

bool MotionController::computePending() {
  if (have_pending_) return true;
  switch (mode_) {
    case Mode::Idle:
      return false;

    case Mode::Tracking: {
      const int32_t p = position();
      if (p + 1 > geom_.maxSteps()) {
        mode_ = Mode::Idle;
        guiding_ = false;
        fault_ = Fault::EndOfTravel;
        return false;
      }
      // Step p -> p+1 when the ideal angle reaches the true angle of p + 0.5.
      const double theta_star = trueAngleAt(double(p) + 0.5);
      for (int guard = 0; guard < 4; ++guard) {
        if (guiding_ && !(omega_ > 0.0)) {
          // Paused for the guide pulse: resume at its end.
          anchor_t_us_ = guide_end_us_;
          omega_ = base_omega_;
          guiding_ = false;
          continue;
        }
        const double t =
            double(anchor_t_us_) + (theta_star - anchor_theta_) / omega_ * 1e6;
        if (guiding_ && t > double(guide_end_us_)) {
          anchor_theta_ += omega_ * double(guide_end_us_ - anchor_t_us_) * 1e-6;
          anchor_t_us_ = guide_end_us_;
          omega_ = base_omega_;
          guiding_ = false;
          continue;
        }
        int64_t ti = int64_t(std::llround(t));
        const int64_t min_t = last_t_ + limits_.min_step_interval_us;
        if (ti < min_t) ti = min_t;
        pending_ = StepEvent{ti, int8_t(1)};
        have_pending_ = true;
        return true;
      }
      return false;
    }

    case Mode::Moving:
    case Mode::Homing: {
      if (move_done_ >= move_.distance()) {
        finishMove();
        return computePending();
      }
      const double t = double(move_start_us_) + move_.timeOfStep(move_done_ + 1) * 1e6;
      int64_t ti = int64_t(std::llround(t));
      const int64_t min_t = last_t_ + limits_.min_step_interval_us;
      if (ti < min_t) ti = min_t;
      pending_ = StepEvent{ti, move_dir_};
      have_pending_ = true;
      return true;
    }
  }
  return false;
}

int64_t MotionController::peekTime() {
  return computePending() ? pending_.t_us : kNever;
}

bool MotionController::next(StepEvent* ev) {
  if (!computePending()) return false;
  *ev = pending_;
  have_pending_ = false;
  raw_ += ev->dir;
  last_t_ = ev->t_us;
  if (mode_ == Mode::Moving || mode_ == Mode::Homing) {
    ++move_done_;
    if (move_done_ >= move_.distance()) finishMove();
  }
  return true;
}

}  // namespace st
