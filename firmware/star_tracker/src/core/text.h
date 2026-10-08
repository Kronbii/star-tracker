// Tiny, allocation-free text helpers shared by the command parser.
#pragma once

#include <cstddef>

namespace st {

inline char toLowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

inline bool equalsIgnoreCase(const char* a, const char* b) {
  if (a == nullptr || b == nullptr) return false;
  while (*a && *b) {
    if (toLowerAscii(*a) != toLowerAscii(*b)) return false;
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

// Parses a decimal number (optional sign, fraction, exponent). The whole token
// must be consumed. Returns false on any junk, so "12abc" is rejected.
bool parseNumber(const char* s, double* out);

}  // namespace st
