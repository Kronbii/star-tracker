#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "../star_tracker/src/core/motion.h"
#include "../star_tracker/src/core/st_math.h"
#include "minitest.h"
#include "sim.h"

using namespace st;

namespace {

constexpr int64_t kSec = 1000000;

struct Rig {
  BarnDoorGeometry g;
  MotionLimits l;
  MotionController c;
  sim::Executor x;
  Rig() : c(g, l), x(c) {}
};

// Max |true angle - ideal angle| over a tracking log, evaluated just before and
// just after every step (where the extremes are). Returns radians.
double maxTrackingError(const Rig& r, const std::vector<sim::Record>& log, size_t from,
                        int64_t t0, double theta0, double omega, bool with_pec) {
  double worst = 0.0;
  const double spr = r.g.stepsPerRev();
  auto trueAngle = [&](int32_t raw) {
    const double s = double(raw - r.c.zeroRaw());
    return r.g.angleForSteps(s) + (with_pec ? r.c.pec().errorRadAt(s, spr) : 0.0);
  };
  for (size_t i = from; i < log.size(); ++i) {
    const double ideal = theta0 + omega * double(log[i].t_us - t0) * 1e-6;
    const double before = trueAngle(log[i].raw_after - log[i].dir) - ideal;
    const double after = trueAngle(log[i].raw_after) - ideal;
    worst = std::max(worst, std::max(std::fabs(before), std::fabs(after)));
  }
  return worst;
}

void checkMonotonic(const std::vector<sim::Record>& log, int64_t min_dt) {
  for (size_t i = 1; i < log.size(); ++i) CHECK(log[i].t_us - log[i - 1].t_us >= min_dt);
}

}  // namespace

TEST(tracking_requires_home) {
  Rig r;
  CHECK(!r.c.startTracking(kSiderealRate, 0));
  CHECK(r.c.fault() == Fault::NotHomed);
  CHECK(r.c.peekTime() == MotionController::kNever);
}

TEST(sidereal_tracking_error_within_half_microstep_for_two_hours) {
  Rig r;
  r.c.setHomeHere(0);
  const int64_t t0 = 5 * kSec;
  r.x.run(t0);
  const double theta0 = r.c.angleRad();
  CHECK(r.c.startTracking(kSiderealRate, t0));
  r.x.run(t0 + 2 * 3600 * kSec);
  CHECK(r.c.mode() == Mode::Tracking);
  const double half_step = 0.5 * r.g.radPerStepAt(0);
  const double err = maxTrackingError(r, r.x.log, 0, t0, theta0, kSiderealRate, false);
  CHECK(err <= half_step * 1.001 + 1e-10);
  // ~0.32 arcsec: quantisation only, no tangent error.
  CHECK(rad2arcsec(err) < 0.35);
  checkMonotonic(r.x.log, r.l.min_step_interval_us);
  // Step rate rises as the boards open (sec^2 law): late intervals are shorter.
  const auto& L = r.x.log;
  const double early = double(L[100].t_us - L[0].t_us) / 100.0;
  const double late = double(L[L.size() - 1].t_us - L[L.size() - 101].t_us) / 100.0;
  CHECK(late < early * 0.9);
}

TEST(lunar_and_solar_rates_move_the_right_angle) {
  for (Rate rate : {Rate::Lunar, Rate::Solar}) {
    Rig r;
    r.c.setHomeHere(0);
    const double w = rateRadPerSec(rate);
    const double th0 = r.c.angleRad();
    CHECK(r.c.startTracking(w, 0));
    r.x.run(3600 * kSec);
    const double moved = r.g.angleForSteps(r.x.raw()) - th0;
    CHECK_NEAR(moved, w * 3600.0, r.g.radPerStepAt(r.x.raw()));
  }
}

TEST(rate_trim_scales_rate) {
  Rig r;
  r.c.setHomeHere(0);
  r.c.setRateTrimPpm(1000.0, 0);
  const double th0 = r.c.angleRad();
  CHECK(r.c.startTracking(kSiderealRate, 0));
  r.x.run(1800 * kSec);
  const double moved = r.g.angleForSteps(r.x.raw()) - th0;
  CHECK_NEAR(moved, kSiderealRate * 1.001 * 1800.0, r.g.radPerStepAt(r.x.raw()));
}

TEST(rate_change_keeps_angle_continuous) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.startTracking(kSiderealRate, 0));
  r.x.run(600 * kSec);
  const double th_switch = r.g.angleForSteps(r.x.raw());
  CHECK(r.c.startTracking(kLunarRate, r.x.now()));
  const size_t from = r.x.log.size();
  const int64_t t_switch = r.x.now();
  r.x.run(1200 * kSec);
  // After the switch the error vs a lunar-rate ideal anchored at the switch is
  // within a microstep (the anchor itself can be up to half a step off).
  const double err =
      maxTrackingError(r, r.x.log, from, t_switch, th_switch, kLunarRate, false);
  CHECK(err <= 1.5 * r.g.radPerStepAt(r.x.raw()));
}

TEST(guide_pulse_shifts_by_expected_angle) {
  for (double mult : {1.5, 0.5, 0.0}) {
    Rig a, b;  // b is the unguided reference
    a.c.setHomeHere(0);
    b.c.setHomeHere(0);
    CHECK(a.c.startTracking(kSiderealRate, 0));
    CHECK(b.c.startTracking(kSiderealRate, 0));
    a.x.run(60 * kSec);
    b.x.run(60 * kSec);
    CHECK(a.c.guide(mult, 2 * kSec, a.x.now()));
    CHECK(a.c.guiding());
    a.x.run(120 * kSec);
    b.x.run(120 * kSec);
    CHECK(!a.c.guiding());
    const double diff = a.g.angleForSteps(a.x.raw()) - b.g.angleForSteps(b.x.raw());
    CHECK_NEAR(diff, (mult - 1.0) * kSiderealRate * 2.0, 1.5 * a.g.radPerStepAt(a.x.raw()));
  }
}

TEST(guide_rejects_bad_arguments) {
  Rig r;
  CHECK(!r.c.guide(1.5, kSec, 0));  // not tracking
  r.c.setHomeHere(0);
  CHECK(r.c.startTracking(kSiderealRate, 0));
  CHECK(!r.c.guide(2.5, kSec, 0));
  CHECK(!r.c.guide(-0.1, kSec, 0));
  CHECK(!r.c.guide(1.5, 0, 0));
}

TEST(pec_cancels_a_known_periodic_error) {
  Rig r;
  r.c.setHomeHere(0);
  // Pretend the screw has a 4 arcsec once-per-rev error; load it as PEC.
  for (int i = 0; i < PecTable::kSize; ++i)
    r.c.pec().set(i, float(4.0 * std::sin(kTwoPi * i / PecTable::kSize)));
  r.c.pec().setEnabled(true);
  const double th0 = r.g.angleForSteps(0) + r.c.pec().errorRadAt(0, r.g.stepsPerRev());
  CHECK(r.c.startTracking(kSiderealRate, 0));
  r.x.run(900 * kSec);  // several screw revolutions
  const double err_true = maxTrackingError(r, r.x.log, 0, 0, th0, kSiderealRate, true);
  // In TRUE angle the corrected drive is still within ~half a microstep.
  CHECK(rad2arcsec(err_true) < 0.45);
  // ...whereas the geometric angle now deliberately deviates by ~ the PE.
  const double err_geom = maxTrackingError(r, r.x.log, 0, 0, th0, kSiderealRate, false);
  CHECK(rad2arcsec(err_geom) > 3.0);
}

TEST(tracking_stops_at_end_of_travel) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.moveTo(r.g.maxSteps() - 20, 0));
  r.x.run(60 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK_EQ(r.c.position(), r.g.maxSteps() - 20);
  CHECK(r.c.trackingRemainingSec() >= 0.0);
  CHECK(r.c.startTracking(kSiderealRate, r.x.now()));
  r.x.run(r.x.now() + 30 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK(r.c.fault() == Fault::EndOfTravel);
  CHECK_EQ(r.c.position(), r.g.maxSteps());
  CHECK(!r.c.startTracking(kSiderealRate, r.x.now()));
}

TEST(move_respects_speed_and_acceleration) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.moveTo(100000, 0));
  r.x.run(40 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK_EQ(r.x.raw(), 100000);
  const auto& L = r.x.log;
  checkMonotonic(L, r.l.min_step_interval_us);
  double vmax_seen = 0.0;
  for (size_t i = 1; i < L.size(); ++i) {
    const double v = 1e6 / double(L[i].t_us - L[i - 1].t_us);
    vmax_seen = std::max(vmax_seen, v);
  }
  // Rounding event times to whole microseconds can raise one interval's
  // apparent speed by ~1%.
  CHECK(vmax_seen <= r.l.max_speed * 1.02);
  CHECK(vmax_seen >= r.l.max_speed * 0.98);
  // Duration ~ D/v + v/a = 12.5 + 0.5 s.
  const double dur = double(L.back().t_us - L.front().t_us) * 1e-6;
  CHECK(dur > 12.5 && dur < 13.3);
}

TEST(stop_during_move_decelerates) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.moveTo(150000, 0));
  r.x.run(3 * kSec);  // cruising at 8000 steps/s
  const int32_t planned_at_stop = r.c.position();
  r.c.stop(r.x.now());
  r.x.run(6 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  const int32_t extra = r.c.position() - planned_at_stop;
  // v^2 / 2a = 2000 steps.
  CHECK(extra >= 1990 && extra <= 2010);
}

TEST(reversal_stops_before_changing_direction) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.moveTo(150000, 0));
  r.x.run(3 * kSec);
  CHECK(r.c.moveTo(10000, r.x.now()));
  r.x.run(40 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK_EQ(r.x.raw(), 10000);
  const auto& L = r.x.log;
  for (size_t i = 1; i < L.size(); ++i) {
    if (L[i].dir != L[i - 1].dir) {
      const double v_before = 1e6 / double(L[i - 1].t_us - L[i - 2].t_us);
      CHECK(v_before <= r.l.start_speed * 1.5);
    }
  }
}

TEST(rewind_returns_to_park) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.startTracking(kSiderealRate, 0));
  r.x.run(600 * kSec);
  CHECK(r.c.rewind(r.x.now()));
  r.x.run(700 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK_EQ(r.c.position(), r.c.parkSteps());
}

TEST(moves_clamp_to_soft_limits) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.moveTo(10000000, 0));
  r.x.run(60 * kSec);
  CHECK_EQ(r.c.position(), r.g.maxSteps());
  CHECK(r.c.moveTo(-500, r.x.now()));
  r.x.run(120 * kSec);
  CHECK_EQ(r.c.position(), 0);
}

TEST(goto_angle) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.gotoAngle(deg2rad(20.0), 0));
  r.x.run(60 * kSec);
  CHECK_NEAR(rad2deg(r.c.angleRad()), 20.0, 1e-3);
}

TEST(homing_finds_switch_and_parks) {
  Rig r;
  // Boards somewhere open: the switch is 50000 steps below the start.
  r.x.setSwitch(true, -50000);
  CHECK(r.c.home(0));
  CHECK(r.c.mode() == Mode::Homing);
  r.x.run(120 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK(r.c.fault() == Fault::None);
  CHECK(r.c.homed());
  // The slow approach stops on the very step that closes the switch.
  CHECK_EQ(r.c.zeroRaw(), -50000);
  CHECK_EQ(r.x.raw() - (-50000), r.l.home_clearance_steps);
  CHECK_EQ(r.c.position(), r.l.home_clearance_steps);
  checkMonotonic(r.x.log, r.l.min_step_interval_us);
}

TEST(homing_from_pressed_switch_backs_off_first) {
  Rig r;
  r.x.setSwitch(true, 0);  // already on the switch
  r.x.run(10000);
  CHECK(r.c.home(r.x.now()));
  r.x.run(60 * kSec);
  CHECK(r.c.homed());
  CHECK_EQ(r.c.zeroRaw(), 0);
  CHECK_EQ(r.x.raw(), r.l.home_clearance_steps);
}

TEST(homing_reports_missing_switch) {
  Rig r;
  CHECK(r.c.home(0));
  r.x.run(200 * kSec);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK(r.c.fault() == Fault::HomeNotFound);
  CHECK(!r.c.homed());
}

TEST(homing_reports_stuck_switch) {
  Rig r;
  r.x.setSwitch(true, 1000000);  // always pressed
  r.x.run(10000);
  CHECK(r.c.home(r.x.now()));
  r.x.run(60 * kSec);
  CHECK(r.c.fault() == Fault::SwitchStuck);
  CHECK(!r.c.homed());
}

TEST(unexpected_switch_hit_while_closing_stops_and_rereferences) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.moveTo(100000, 0));
  r.x.run(20 * kSec);
  r.x.setSwitch(true, 60000);  // switch "moved" (e.g. lost steps)
  CHECK(r.c.moveTo(0, r.x.now()));
  r.x.run(60 * kSec);
  CHECK(r.c.fault() == Fault::LimitHit);
  CHECK(r.c.mode() == Mode::Idle);
  CHECK(r.c.position() < 0);       // overshoot below the new zero while stopping
  // Bounded by the stopping distance (2000) plus the 20 ms look-ahead already
  // queued at 8000 steps/s (160) and one loop of detection latency.
  CHECK(r.c.position() > -2200);
}

TEST(unhomed_jog_is_limited) {
  Rig r;
  CHECK(!r.c.moveTo(100, 0));
  CHECK(r.c.moveBy(1600, 0));
  r.x.run(2 * kSec);
  CHECK_EQ(r.x.raw(), 1600);
  CHECK(!r.c.moveBy(r.l.max_jog_steps + 1, r.x.now()));
}

TEST(configure_only_when_idle) {
  Rig r;
  r.c.setHomeHere(0);
  CHECK(r.c.startTracking(kSiderealRate, 0));
  BarnDoorGeometry g2;
  g2.radius_mm = 210.0;
  CHECK(!r.c.configure(g2, r.l));
  r.c.stop(0);
  CHECK(r.c.configure(g2, r.l));
  CHECK_NEAR(r.c.geometry().radius_mm, 210.0, 0.0);
}
