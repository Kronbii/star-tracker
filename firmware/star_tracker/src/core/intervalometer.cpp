#include "intervalometer.h"

namespace st {

const char* shotStateName(ShotState s) {
  switch (s) {
    case ShotState::Idle:
      return "idle";
    case ShotState::Delay:
      return "delay";
    case ShotState::Prefocus:
      return "prefocus";
    case ShotState::Exposing:
      return "exposing";
    case ShotState::Gap:
      return "gap";
    case ShotState::Done:
      return "done";
  }
  return "?";
}

bool ShootPlan::valid() const {
  constexpr uint32_t kHourMs = 3600u * 1000u;
  if (frames < 1 || frames > 9999) return false;
  if (exposure_ms < 1 || exposure_ms > kHourMs) return false;
  if (gap_ms > kHourMs || delay_ms > kHourMs || prefocus_ms > 10000) return false;
  if (!bulb && (pulse_ms < 20 || pulse_ms > exposure_ms)) return false;
  return true;
}

uint64_t ShootPlan::totalMs() const {
  const uint64_t n = frames;
  return uint64_t(delay_ms) + n * (uint64_t(prefocus_ms) + exposure_ms) +
         (n > 0 ? (n - 1) * uint64_t(gap_ms) : 0);
}

bool Intervalometer::start(const ShootPlan& plan, int64_t now_us) {
  if (!plan.valid()) return false;
  plan_ = plan;
  done_ = 0;
  seq_start_us_ = now_us;
  enter(ShotState::Delay, now_us);
  update(now_us);
  return true;
}

void Intervalometer::abort() {
  state_ = ShotState::Idle;
  shutter_ = false;
  focus_ = false;
  frame_edge_ = false;
}

void Intervalometer::enter(ShotState s, int64_t t_us) {
  state_ = s;
  state_start_us_ = t_us;
}

void Intervalometer::update(int64_t now_us) {
  frame_edge_ = false;
  // Transitions use the scheduled end of the previous state, not `now`, so
  // late update() calls never stretch or shrink an exposure.
  for (int guard = 0; guard < 16 && active(); ++guard) {
    uint32_t dur_ms = 0;
    switch (state_) {
      case ShotState::Delay:
        dur_ms = plan_.delay_ms;
        break;
      case ShotState::Prefocus:
        dur_ms = plan_.prefocus_ms;
        break;
      case ShotState::Exposing:
        dur_ms = plan_.exposure_ms;
        break;
      case ShotState::Gap:
        dur_ms = plan_.gap_ms;
        break;
      default:
        break;
    }
    const int64_t end = state_start_us_ + int64_t(dur_ms) * 1000;
    if (now_us < end) break;
    switch (state_) {
      case ShotState::Delay:
      case ShotState::Gap:
        enter(ShotState::Prefocus, end);
        break;
      case ShotState::Prefocus:
        enter(ShotState::Exposing, end);
        break;
      case ShotState::Exposing:
        ++done_;
        frame_edge_ = true;
        enter(done_ >= plan_.frames ? ShotState::Done : ShotState::Gap, end);
        break;
      default:
        break;
    }
  }
  if (state_ == ShotState::Exposing) {
    shutter_ = plan_.bulb || (now_us - state_start_us_) < int64_t(plan_.pulse_ms) * 1000;
  } else {
    shutter_ = false;
  }
  focus_ = state_ == ShotState::Prefocus || shutter_;
}

uint64_t Intervalometer::remainingMs(int64_t now_us) const {
  if (!active()) return 0;
  const int64_t end = seq_start_us_ + int64_t(plan_.totalMs()) * 1000;
  return now_us >= end ? 0 : uint64_t((end - now_us) / 1000);
}

}  // namespace st
