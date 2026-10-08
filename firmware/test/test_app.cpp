#include <cstring>
#include <string>

#include "../star_tracker/src/core/app.h"
#include "../star_tracker/src/core/json_writer.h"
#include "../star_tracker/src/core/st_math.h"
#include "minitest.h"
#include "sim.h"

using namespace st;

namespace {

constexpr int64_t kSec = 1000000;

struct AppRig {
  TrackerApp app;
  sim::Executor x;
  char buf[1024];
  AppRig() : x(app.motion()) { app.begin(Settings{}); }
  std::string cmd(const char* line) {
    app.handle(line, x.now(), x.raw(), buf, sizeof(buf));
    return buf;
  }
  void run(int64_t until) {
    // Interleave executor and app ticks at 1 ms.
    while (x.now() < until) {
      x.run(x.now() + 1000);
      app.tick(x.now());
    }
  }
};

bool has(const std::string& s, const char* sub) { return s.find(sub) != std::string::npos; }

}  // namespace

TEST(app_rejects_unknown_and_reports_errors) {
  AppRig r;
  CHECK(has(r.cmd("fly"), "\"ok\":false"));
  CHECK(has(r.cmd("fly"), "unknown command"));
  // Tracking before homing is refused with the fault name.
  const std::string s = r.cmd("track");
  CHECK(has(s, "\"ok\":false"));
  CHECK(has(s, "not_homed"));
}

TEST(app_full_session_flow) {
  AppRig r;
  r.x.setSwitch(true, -40000);
  CHECK(has(r.cmd("home"), "\"ok\":true"));
  r.run(90 * kSec);
  std::string st = r.cmd("status");
  CHECK(has(st, "\"homed\":true"));
  CHECK(has(st, "\"mode\":\"idle\""));
  CHECK(has(r.cmd("time 1794081600000"), "\"ok\":true"));
  CHECK(has(r.cmd("track sidereal"), "\"ok\":true"));
  CHECK(has(r.cmd("shoot 3 2 1 0"), "\"ok\":true"));
  r.run(r.x.now() + 2500000);
  st = r.cmd("status");
  CHECK(has(st, "\"mode\":\"tracking\""));
  CHECK(has(st, "\"rate\":\"sidereal\""));
  CHECK(has(st, "\"rate_arcsec_s\":15.0411"));
  CHECK(has(st, "\"polaris\":{\"clock\":"));
  CHECK(has(st, "\"state\":\"gap\""));
  r.run(r.x.now() + 10 * kSec);
  st = r.cmd("status");
  CHECK(has(st, "\"done\":3"));
  CHECK(has(st, "\"state\":\"done\""));
  CHECK(has(r.cmd("stop"), "\"ok\":true"));
  CHECK(has(r.cmd("rewind"), "\"ok\":true"));
  r.run(r.x.now() + 30 * kSec);
  CHECK_EQ(r.app.motion().position(), r.app.motion().parkSteps());
}

TEST(app_settings_and_save_handshake) {
  AppRig r;
  CHECK(has(r.cmd("set radius 201.5"), "\"ok\":true"));
  CHECK_NEAR(r.app.settings().geom.radius_mm, 201.5, 0);
  CHECK_NEAR(r.app.motion().geometry().radius_mm, 201.5, 0);
  CHECK(has(r.cmd("set offset 300"), "\"ok\":false"));  // parser range
  CHECK(has(r.cmd("set lon 35.48"), "\"ok\":true"));
  CHECK_NEAR(r.app.settings().lon_deg, 35.48, 0);
  CHECK(!r.app.takeSaveRequest());
  CHECK(has(r.cmd("save"), "\"ok\":true"));
  CHECK(r.app.takeSaveRequest());
  CHECK(!r.app.takeSaveRequest());
  // Geometry changes are refused while tracking.
  r.cmd("sethome");
  CHECK(has(r.cmd("track"), "\"ok\":true"));
  CHECK(has(r.cmd("set radius 199"), "stop first"));
  CHECK(has(r.cmd("pec 3 1.5"), "stop first"));
  CHECK(has(r.cmd("set lon 35.5"), "\"ok\":true"));  // harmless while tracking
}

TEST(app_pec_commands_reach_controller) {
  AppRig r;
  CHECK(has(r.cmd("pec 5 2.25"), "\"ok\":true"));
  CHECK_NEAR(r.app.motion().pec().get(5), 2.25, 1e-6);
  CHECK_NEAR(r.app.settings().pec[5], 2.25, 1e-6);
  CHECK(has(r.cmd("pec on"), "\"ok\":true"));
  CHECK(r.app.motion().pec().enabled());
  CHECK(has(r.cmd("pec clear"), "\"ok\":true"));
  CHECK_NEAR(r.app.motion().pec().get(5), 0.0, 0);
}

TEST(app_aborts_sequence_when_travel_runs_out) {
  AppRig r;
  r.cmd("sethome");
  r.app.motion().moveTo(r.app.motion().geometry().maxSteps() - 30, 0);
  r.run(60 * kSec);
  CHECK(has(r.cmd("track"), "\"ok\":true"));
  CHECK(has(r.cmd("shoot 50 30"), "\"ok\":true"));
  r.run(r.x.now() + 10 * kSec);
  CHECK(r.app.motion().fault() == Fault::EndOfTravel);
  CHECK(!r.app.camera().active());
  CHECK(r.app.shotStoppedByTravel());
  // A test frame afterwards (not tracking) is allowed.
  CHECK(has(r.cmd("test 1"), "\"ok\":true"));
  CHECK(r.app.camera().active());
}

TEST(app_guide_and_trim) {
  AppRig r;
  r.cmd("sethome");
  CHECK(has(r.cmd("guide 500"), "not tracking"));
  r.cmd("track");
  CHECK(has(r.cmd("guide 500 0.5"), "\"ok\":true"));
  CHECK(r.app.motion().guiding());
  CHECK(has(r.cmd("trim 250"), "\"ok\":true"));
  CHECK_NEAR(r.app.motion().rateTrimPpm(), 250.0, 0);
  CHECK_NEAR(r.app.motion().trackingRate(), kSiderealRate * 1.00025, 1e-15);
}

TEST(json_writer_escapes_and_truncates) {
  char b[64];
  JsonWriter w(b, sizeof(b));
  w.beginObject().field("s", "a\"b\\c\n").field("x", 1.5, 2).fieldBool("t", true).endObject();
  CHECK(w.ok());
  CHECK(std::strcmp(b, "{\"s\":\"a\\\"b\\\\c\\u000a\",\"x\":1.50,\"t\":true}") == 0);
  char small[8];
  JsonWriter w2(small, sizeof(small));
  w2.beginObject().field("long_key", "long value").endObject();
  CHECK(!w2.ok());
  CHECK(std::strlen(small) < sizeof(small));
  char nanbuf[32];
  JsonWriter w3(nanbuf, sizeof(nanbuf));
  w3.beginObject().field("v", std::nan("")).endObject();
  CHECK(std::strcmp(nanbuf, "{\"v\":null}") == 0);
}
