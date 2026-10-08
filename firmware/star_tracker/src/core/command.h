// Text command language shared by the serial console and the web API.
//
//   status                         report state as JSON
//   track [sidereal|lunar|solar|<arcsec/s>]
//   stop                           stop tracking / decelerate a move
//   rewind                         return to the park position (fast)
//   home                           run the homing cycle on the switch
//   sethome                        declare the current position as home (bench)
//   goto <deg>                     move to a board angle
//   jog <mm>                       move the nut by +-mm
//   guide <ms> [multiplier]        pulse guide: rate x multiplier (0..2), default 1.5
//   trim <ppm>                     drive-rate calibration
//   shoot <n> <exp_s> [gap_s] [delay_s]   BULB sequence
//   shoot stop                     abort the sequence
//   test [exp_s]                   one test frame (default 5 s)
//   time <unix_ms>                 set the clock (sent by the web UI)
//   set <key> <value>              radius|offset|lead|home|max|lon (mm, deg)
//   pec on|off|clear | pec <index> <arcsec>
//   save                           persist settings to flash
//   help
#pragma once

#include <cstdint>

#include "st_math.h"

namespace st {

enum class CmdType : uint8_t {
  Invalid = 0,
  Help,
  Status,
  Track,
  Stop,
  Rewind,
  Home,
  SetHome,
  Goto,
  Jog,
  Guide,
  Trim,
  Shoot,
  ShootStop,
  TestShot,
  Time,
  Set,
  PecClear,
  PecEnable,
  PecSet,
  Save,
};

enum class SetKey : uint8_t { None = 0, Radius, Offset, Lead, HomeHeight, MaxHeight, Longitude };

struct Command {
  CmdType type = CmdType::Invalid;
  Rate rate = Rate::Sidereal;
  double a = 0.0, b = 0.0, c = 0.0, d = 0.0;  // numeric arguments
  int64_t i64 = 0;                            // integer argument (time)
  SetKey key = SetKey::None;
  bool flag = false;                          // pec on/off
  const char* error = nullptr;                // static message when Invalid
};

// Parses one line (leading/trailing spaces ignored, case-insensitive
// keywords). Returns false and sets out->error for invalid input.
bool parseCommand(const char* line, Command* out);

const char* setKeyName(SetKey k);

}  // namespace st
