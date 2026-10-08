#include "command.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "text.h"

namespace st {

namespace {

constexpr int kMaxTokens = 6;
constexpr int kMaxLine = 96;

struct Tokens {
  char buf[kMaxLine + 1];
  const char* tok[kMaxTokens];
  int n = 0;
  bool overflow = false;
};

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

void tokenize(const char* line, Tokens* t) {
  t->n = 0;
  t->overflow = false;
  size_t len = std::strlen(line);
  if (len > size_t(kMaxLine)) {
    t->overflow = true;
    return;
  }
  std::memcpy(t->buf, line, len + 1);
  char* p = t->buf;
  while (*p) {
    while (*p && isSpace(*p)) *p++ = '\0';
    if (!*p) break;
    if (t->n == kMaxTokens) {
      t->overflow = true;
      return;
    }
    t->tok[t->n++] = p;
    while (*p && !isSpace(*p)) ++p;
  }
}

bool fail(Command* out, const char* msg) {
  out->type = CmdType::Invalid;
  out->error = msg;
  return false;
}

bool num(const char* s, double* v) { return parseNumber(s, v); }

bool parseInt64(const char* s, int64_t* v) {
  if (s == nullptr || *s == '\0') return false;
  char* end = nullptr;
  const long long x = std::strtoll(s, &end, 10);
  if (*end != '\0') return false;
  *v = int64_t(x);
  return true;
}

}  // namespace

const char* setKeyName(SetKey k) {
  switch (k) {
    case SetKey::Radius:
      return "radius";
    case SetKey::Offset:
      return "offset";
    case SetKey::Lead:
      return "lead";
    case SetKey::HomeHeight:
      return "home";
    case SetKey::MaxHeight:
      return "max";
    case SetKey::Longitude:
      return "lon";
    case SetKey::None:
      break;
  }
  return "?";
}

bool parseCommand(const char* line, Command* out) {
  *out = Command{};
  if (line == nullptr) return fail(out, "empty");
  Tokens t;
  tokenize(line, &t);
  if (t.overflow) return fail(out, "line too long");
  if (t.n == 0) return fail(out, "empty");
  const char* cmd = t.tok[0];
  const int argc = t.n - 1;
  auto arg = [&](int i) { return t.tok[i + 1]; };

  if (equalsIgnoreCase(cmd, "help") || equalsIgnoreCase(cmd, "?")) {
    out->type = CmdType::Help;
  } else if (equalsIgnoreCase(cmd, "status")) {
    out->type = CmdType::Status;
  } else if (equalsIgnoreCase(cmd, "track") || equalsIgnoreCase(cmd, "start")) {
    out->type = CmdType::Track;
    out->rate = Rate::Sidereal;
    if (argc >= 1) {
      if (!parseRateName(arg(0), &out->rate)) {
        if (!num(arg(0), &out->a) || !(out->a > 0.0) || out->a > 60.0)
          return fail(out, "rate: sidereal|lunar|solar|<arcsec/s 0..60>");
        out->rate = Rate::Custom;
      }
    }
    if (argc > 1) return fail(out, "track takes one argument");
  } else if (equalsIgnoreCase(cmd, "stop")) {
    out->type = CmdType::Stop;
  } else if (equalsIgnoreCase(cmd, "rewind")) {
    out->type = CmdType::Rewind;
  } else if (equalsIgnoreCase(cmd, "home")) {
    out->type = CmdType::Home;
  } else if (equalsIgnoreCase(cmd, "sethome")) {
    out->type = CmdType::SetHome;
  } else if (equalsIgnoreCase(cmd, "goto")) {
    out->type = CmdType::Goto;
    if (argc != 1 || !num(arg(0), &out->a) || out->a < 0.0 || out->a > 60.0)
      return fail(out, "goto <deg 0..60>");
  } else if (equalsIgnoreCase(cmd, "jog")) {
    out->type = CmdType::Jog;
    if (argc != 1 || !num(arg(0), &out->a) || std::fabs(out->a) > 200.0)
      return fail(out, "jog <mm -200..200>");
  } else if (equalsIgnoreCase(cmd, "guide")) {
    out->type = CmdType::Guide;
    out->b = 1.5;
    if (argc < 1 || argc > 2 || !num(arg(0), &out->a) || !(out->a > 0.0) || out->a > 60000.0)
      return fail(out, "guide <ms 1..60000> [multiplier 0..2]");
    if (argc == 2 && (!num(arg(1), &out->b) || out->b < 0.0 || out->b > 2.0))
      return fail(out, "guide multiplier must be 0..2");
  } else if (equalsIgnoreCase(cmd, "trim")) {
    out->type = CmdType::Trim;
    if (argc != 1 || !num(arg(0), &out->a) || std::fabs(out->a) > 50000.0)
      return fail(out, "trim <ppm -50000..50000>");
  } else if (equalsIgnoreCase(cmd, "shoot")) {
    if (argc == 1 && (equalsIgnoreCase(arg(0), "stop") || equalsIgnoreCase(arg(0), "abort"))) {
      out->type = CmdType::ShootStop;
    } else {
      out->type = CmdType::Shoot;
      out->c = 3.0;  // gap s
      out->d = 2.0;  // delay s
      if (argc < 2 || argc > 4) return fail(out, "shoot <n> <exp_s> [gap_s] [delay_s]");
      if (!num(arg(0), &out->a) || out->a < 1.0 || out->a > 9999.0 ||
          out->a != std::floor(out->a))
        return fail(out, "frames must be an integer 1..9999");
      if (!num(arg(1), &out->b) || out->b < 0.1 || out->b > 3600.0)
        return fail(out, "exposure must be 0.1..3600 s");
      if (argc >= 3 && (!num(arg(2), &out->c) || out->c < 0.0 || out->c > 3600.0))
        return fail(out, "gap must be 0..3600 s");
      if (argc >= 4 && (!num(arg(3), &out->d) || out->d < 0.0 || out->d > 3600.0))
        return fail(out, "delay must be 0..3600 s");
    }
  } else if (equalsIgnoreCase(cmd, "test")) {
    out->type = CmdType::TestShot;
    out->a = 5.0;
    if (argc > 1 || (argc == 1 && (!num(arg(0), &out->a) || out->a < 0.1 || out->a > 600.0)))
      return fail(out, "test [exp_s 0.1..600]");
  } else if (equalsIgnoreCase(cmd, "time")) {
    out->type = CmdType::Time;
    // Sanity window: 2020..2100.
    if (argc != 1 || !parseInt64(arg(0), &out->i64) || out->i64 < 1577836800000LL ||
        out->i64 > 4102444800000LL)
      return fail(out, "time <unix_ms>");
  } else if (equalsIgnoreCase(cmd, "set")) {
    out->type = CmdType::Set;
    if (argc != 2 || !num(arg(1), &out->a)) return fail(out, "set <key> <value>");
    const char* k = arg(0);
    if (equalsIgnoreCase(k, "radius")) {
      out->key = SetKey::Radius;
      if (out->a < 50.0 || out->a > 1000.0) return fail(out, "radius 50..1000 mm");
    } else if (equalsIgnoreCase(k, "offset")) {
      out->key = SetKey::Offset;
      if (out->a < 0.0 || out->a > 50.0) return fail(out, "offset 0..50 mm");
    } else if (equalsIgnoreCase(k, "lead")) {
      out->key = SetKey::Lead;
      if (out->a < 0.2 || out->a > 20.0) return fail(out, "lead 0.2..20 mm");
    } else if (equalsIgnoreCase(k, "home")) {
      out->key = SetKey::HomeHeight;
      if (out->a < -20.0 || out->a > 100.0) return fail(out, "home -20..100 mm");
    } else if (equalsIgnoreCase(k, "max")) {
      out->key = SetKey::MaxHeight;
      if (out->a < 10.0 || out->a > 400.0) return fail(out, "max 10..400 mm");
    } else if (equalsIgnoreCase(k, "lon")) {
      out->key = SetKey::Longitude;
      if (out->a < -180.0 || out->a > 180.0) return fail(out, "lon -180..180 deg");
    } else {
      return fail(out, "keys: radius offset lead home max lon");
    }
  } else if (equalsIgnoreCase(cmd, "pec")) {
    if (argc == 1 && equalsIgnoreCase(arg(0), "clear")) {
      out->type = CmdType::PecClear;
    } else if (argc == 1 && equalsIgnoreCase(arg(0), "on")) {
      out->type = CmdType::PecEnable;
      out->flag = true;
    } else if (argc == 1 && equalsIgnoreCase(arg(0), "off")) {
      out->type = CmdType::PecEnable;
      out->flag = false;
    } else if (argc == 2) {
      out->type = CmdType::PecSet;
      if (!num(arg(0), &out->a) || out->a < 0.0 || out->a > 63.0 ||
          out->a != std::floor(out->a))
        return fail(out, "pec index 0..63");
      if (!num(arg(1), &out->b) || std::fabs(out->b) > 600.0)
        return fail(out, "pec value -600..600 arcsec");
    } else {
      return fail(out, "pec on|off|clear|<index> <arcsec>");
    }
  } else if (equalsIgnoreCase(cmd, "save")) {
    out->type = CmdType::Save;
  } else {
    return fail(out, "unknown command (try help)");
  }

  // Commands that take no arguments.
  switch (out->type) {
    case CmdType::Help:
    case CmdType::Status:
    case CmdType::Stop:
    case CmdType::Rewind:
    case CmdType::Home:
    case CmdType::SetHome:
    case CmdType::Save:
      if (argc != 0) return fail(out, "command takes no arguments");
      break;
    default:
      break;
  }
  return true;
}

}  // namespace st
