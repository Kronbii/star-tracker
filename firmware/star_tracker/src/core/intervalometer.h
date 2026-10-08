// Intervalometer for a Canon body on the E3 (2.5 mm TRS) remote port.
//
// The firmware closes two optocouplers: FOCUS (ring) and SHUTTER (tip), both
// to ground (sleeve). Like Canon's RS-60E3, focus is closed first, then the
// shutter; both are held for the exposure in BULB mode. In "camera timed"
// mode the shutter is pulsed and the camera's own shutter speed applies.
//
//   delay -> [prefocus -> expose -> gap] x frames -> done
#pragma once

#include <cstdint>

namespace st {

struct ShootPlan {
  uint16_t frames = 1;
  uint32_t exposure_ms = 30000;  // BULB: shutter held this long
  uint32_t gap_ms = 3000;        // between frames (buffer write, settling)
  uint32_t delay_ms = 2000;      // before the first frame (vibration damping)
  uint32_t prefocus_ms = 200;    // focus line closed before the shutter
  bool bulb = true;              // false: pulse the shutter, camera times it
  uint32_t pulse_ms = 150;       // shutter pulse length when !bulb

  bool valid() const;
  // Wall-clock length of the whole sequence (ms).
  uint64_t totalMs() const;
};

enum class ShotState : uint8_t { Idle = 0, Delay, Prefocus, Exposing, Gap, Done };
const char* shotStateName(ShotState s);

class Intervalometer {
 public:
  bool start(const ShootPlan& plan, int64_t now_us);
  void abort();
  // Advance the state machine; call often (every loop).
  void update(int64_t now_us);

  bool shutterClosed() const { return shutter_; }
  bool focusClosed() const { return focus_; }
  ShotState state() const { return state_; }
  bool active() const { return state_ != ShotState::Idle && state_ != ShotState::Done; }
  uint16_t framesDone() const { return done_; }
  uint16_t framesTotal() const { return plan_.frames; }
  // Fires once per completed frame (true for one update() call).
  bool frameJustCompleted() const { return frame_edge_; }
  const ShootPlan& plan() const { return plan_; }
  // Milliseconds until the sequence ends (0 when idle).
  uint64_t remainingMs(int64_t now_us) const;

 private:
  void enter(ShotState s, int64_t t_us);

  ShootPlan plan_{};
  ShotState state_ = ShotState::Idle;
  int64_t state_start_us_ = 0;
  int64_t seq_start_us_ = 0;
  uint16_t done_ = 0;
  bool shutter_ = false;
  bool focus_ = false;
  bool frame_edge_ = false;
};

}  // namespace st
