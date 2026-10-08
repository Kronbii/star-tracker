// Runs the real controller (the same code the ESP32 runs) through a tracking
// session on the host and writes a CSV the Python tools use for the
// tangent-correction figure and a cross-language consistency test.
//
//   track_sim [hours] [sample_s] > track.csv
//
// Columns: t_s, steps, angle_deg, err_arcsec (firmware schedule vs ideal),
//          uncorrected_err_arcsec (constant screw speed matched at start)
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "../star_tracker/src/core/motion.h"
#include "../star_tracker/src/core/st_math.h"
#include "sim.h"

int main(int argc, char** argv) {
  const double hours = argc > 1 ? std::atof(argv[1]) : 2.0;
  const double sample_s = argc > 2 ? std::atof(argv[2]) : 10.0;
  st::BarnDoorGeometry g;
  st::MotionLimits l;
  st::MotionController c(g, l);
  sim::Executor x(c);
  c.setHomeHere(0);
  const double th0 = c.angleRad();
  c.startTracking(st::kSiderealRate, 0);
  const double v0 = st::kSiderealRate / g.radPerStepAt(0);  // steps/s at start
  std::printf("t_s,steps,angle_deg,err_arcsec,uncorrected_err_arcsec\n");
  const int64_t end = int64_t(hours * 3600.0 * 1e6);
  const int64_t dt = int64_t(sample_s * 1e6);
  for (int64_t t = 0; t <= end; t += dt) {
    x.run(t);
    if (c.mode() != st::Mode::Tracking) break;
    const double ideal = th0 + st::kSiderealRate * double(t) * 1e-6;
    const double actual = g.angleForSteps(x.raw());
    const double uncorrected = g.angleForSteps(v0 * double(t) * 1e-6);
    std::printf("%.1f,%d,%.6f,%.4f,%.3f\n", double(t) * 1e-6, x.raw(), st::rad2deg(actual),
                st::rad2arcsec(actual - ideal), st::rad2arcsec(uncorrected - ideal));
  }
  return 0;
}
