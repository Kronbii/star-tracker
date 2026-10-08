#include "text.h"

#include <cmath>
#include <cstdlib>

namespace st {

bool parseNumber(const char* s, double* out) {
  if (s == nullptr || *s == '\0') return false;
  char* end = nullptr;
  const double v = std::strtod(s, &end);
  if (end == s || *end != '\0') return false;
  if (!std::isfinite(v)) return false;
  *out = v;
  return true;
}

}  // namespace st
