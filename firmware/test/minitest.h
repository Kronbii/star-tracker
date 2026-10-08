// Minimal dependency-free unit-test harness (no network needed to build).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace minitest {

struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}

inline int& failures() {
  static int f = 0;
  return f;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, fn}); }
};

inline void report(const char* file, int line, const std::string& msg) {
  std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg.c_str());
  ++failures();
}

inline int runAll(const char* filter) {
  int ran = 0;
  for (const auto& c : registry()) {
    if (filter && std::string(c.name).find(filter) == std::string::npos) continue;
    const int before = failures();
    c.fn();
    ++ran;
    std::printf("%s %s\n", failures() == before ? "[ OK ]" : "[FAIL]", c.name);
  }
  std::printf("\n%d tests, %d failed checks\n", ran, failures());
  return failures() == 0 ? 0 : 1;
}

}  // namespace minitest

#define MT_CAT2(a, b) a##b
#define MT_CAT(a, b) MT_CAT2(a, b)
#define TEST(name)                                                              \
  static void name();                                                           \
  static minitest::Registrar MT_CAT(reg_, name)(#name, name);                   \
  static void name()

#define CHECK(cond)                                                             \
  do {                                                                          \
    if (!(cond)) minitest::report(__FILE__, __LINE__, "CHECK(" #cond ")");      \
  } while (0)

#define CHECK_EQ(a, b)                                                          \
  do {                                                                          \
    const auto _a = (a);                                                        \
    const auto _b = (b);                                                        \
    if (!(_a == _b))                                                            \
      minitest::report(__FILE__, __LINE__,                                      \
                       std::string(#a " == " #b " (") + std::to_string(_a) +    \
                           " vs " + std::to_string(_b) + ")");                  \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                   \
  do {                                                                          \
    const double _a = double(a);                                                \
    const double _b = double(b);                                                \
    if (!(std::fabs(_a - _b) <= (tol)))                                         \
      minitest::report(__FILE__, __LINE__,                                      \
                       std::string(#a " ~= " #b " (") + std::to_string(_a) +    \
                           " vs " + std::to_string(_b) + ", tol " +             \
                           std::to_string(double(tol)) + ")");                  \
  } while (0)
