#include <cstring>

#include "../star_tracker/src/core/command.h"
#include "minitest.h"

using namespace st;

namespace {
Command parse(const char* s) {
  Command c;
  parseCommand(s, &c);
  return c;
}
}  // namespace

TEST(parse_track_variants) {
  CHECK(parse("track").type == CmdType::Track);
  CHECK(parse("track").rate == Rate::Sidereal);
  CHECK(parse("  TRACK   lunar ").rate == Rate::Lunar);
  CHECK(parse("track sun").rate == Rate::Solar);
  Command c = parse("track 15.2");
  CHECK(c.type == CmdType::Track && c.rate == Rate::Custom);
  CHECK_NEAR(c.a, 15.2, 1e-12);
  CHECK(parse("track 0").type == CmdType::Invalid);
  CHECK(parse("track fast").type == CmdType::Invalid);
  CHECK(parse("track lunar extra").type == CmdType::Invalid);
}

TEST(parse_simple_commands) {
  CHECK(parse("stop").type == CmdType::Stop);
  CHECK(parse("rewind").type == CmdType::Rewind);
  CHECK(parse("home").type == CmdType::Home);
  CHECK(parse("sethome").type == CmdType::SetHome);
  CHECK(parse("status").type == CmdType::Status);
  CHECK(parse("save").type == CmdType::Save);
  CHECK(parse("help").type == CmdType::Help);
  CHECK(parse("stop now").type == CmdType::Invalid);
  CHECK(parse("").type == CmdType::Invalid);
  CHECK(parse("   ").type == CmdType::Invalid);
  CHECK(parse("launch").type == CmdType::Invalid);
}

TEST(parse_motion_arguments) {
  Command c = parse("goto 12.5");
  CHECK(c.type == CmdType::Goto);
  CHECK_NEAR(c.a, 12.5, 1e-12);
  CHECK(parse("goto 75").type == CmdType::Invalid);
  c = parse("jog -2.5");
  CHECK(c.type == CmdType::Jog);
  CHECK_NEAR(c.a, -2.5, 1e-12);
  CHECK(parse("jog 2.5mm").type == CmdType::Invalid);
  c = parse("guide 500");
  CHECK(c.type == CmdType::Guide);
  CHECK_NEAR(c.a, 500, 0);
  CHECK_NEAR(c.b, 1.5, 0);
  c = parse("guide 250 0.5");
  CHECK_NEAR(c.b, 0.5, 0);
  CHECK(parse("guide 250 3").type == CmdType::Invalid);
  c = parse("trim -120");
  CHECK(c.type == CmdType::Trim);
  CHECK_NEAR(c.a, -120, 0);
}

TEST(parse_shoot) {
  Command c = parse("shoot 40 60");
  CHECK(c.type == CmdType::Shoot);
  CHECK_NEAR(c.a, 40, 0);
  CHECK_NEAR(c.b, 60, 0);
  CHECK_NEAR(c.c, 3, 0);
  CHECK_NEAR(c.d, 2, 0);
  c = parse("shoot 10 30 5 10");
  CHECK_NEAR(c.c, 5, 0);
  CHECK_NEAR(c.d, 10, 0);
  CHECK(parse("shoot 2.5 30").type == CmdType::Invalid);
  CHECK(parse("shoot 10").type == CmdType::Invalid);
  CHECK(parse("shoot stop").type == CmdType::ShootStop);
  CHECK(parse("shoot abort").type == CmdType::ShootStop);
  c = parse("test");
  CHECK(c.type == CmdType::TestShot);
  CHECK_NEAR(c.a, 5, 0);
  CHECK_NEAR(parse("test 2").a, 2, 0);
}

TEST(parse_time_set_pec) {
  Command c = parse("time 1791504000000");
  CHECK(c.type == CmdType::Time);
  CHECK_EQ(c.i64, int64_t(1791504000000LL));
  CHECK(parse("time 12").type == CmdType::Invalid);
  CHECK(parse("time 1791504000000.5").type == CmdType::Invalid);
  c = parse("set radius 200.35");
  CHECK(c.type == CmdType::Set && c.key == SetKey::Radius);
  CHECK_NEAR(c.a, 200.35, 1e-12);
  CHECK(parse("set lon 35.5").key == SetKey::Longitude);
  CHECK(parse("set radius 2").type == CmdType::Invalid);
  CHECK(parse("set colour 2").type == CmdType::Invalid);
  CHECK(parse("pec on").type == CmdType::PecEnable && parse("pec on").flag);
  CHECK(parse("pec off").type == CmdType::PecEnable && !parse("pec off").flag);
  CHECK(parse("pec clear").type == CmdType::PecClear);
  c = parse("pec 12 -3.5");
  CHECK(c.type == CmdType::PecSet);
  CHECK_NEAR(c.a, 12, 0);
  CHECK_NEAR(c.b, -3.5, 1e-12);
  CHECK(parse("pec 64 1").type == CmdType::Invalid);
}

TEST(parse_rejects_overlong_lines) {
  char line[200];
  std::memset(line, 'a', sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  Command c;
  CHECK(!parseCommand(line, &c));
  CHECK(c.error != nullptr);
  CHECK(!parseCommand("a b c d e f g", &c));
  CHECK(!parseCommand(nullptr, &c));
}
