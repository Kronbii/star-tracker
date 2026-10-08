#include "json_writer.h"

#include <cmath>
#include <cstdio>

namespace st {

JsonWriter::JsonWriter(char* buf, size_t cap) : buf_(buf), cap_(cap) {
  if (cap_ > 0) buf_[0] = '\0';
  else ok_ = false;
}

void JsonWriter::raw(const char* s) {
  while (*s) {
    if (len_ + 1 >= cap_) {
      ok_ = false;
      return;
    }
    buf_[len_++] = *s++;
    buf_[len_] = '\0';
  }
}

void JsonWriter::escaped(const char* s) {
  raw("\"");
  char tmp[8];
  for (; s && *s; ++s) {
    const unsigned char c = static_cast<unsigned char>(*s);
    if (c == '"' || c == '\\') {
      tmp[0] = '\\';
      tmp[1] = char(c);
      tmp[2] = '\0';
    } else if (c < 0x20) {
      std::snprintf(tmp, sizeof(tmp), "\\u%04x", unsigned(c));
    } else {
      tmp[0] = char(c);
      tmp[1] = '\0';
    }
    raw(tmp);
  }
  raw("\"");
}

void JsonWriter::key(const char* k) {
  if (need_comma_) raw(",");
  if (k) {
    escaped(k);
    raw(":");
  }
  need_comma_ = true;
}

JsonWriter& JsonWriter::beginObject(const char* k) {
  key(k);
  raw("{");
  need_comma_ = false;
  return *this;
}

JsonWriter& JsonWriter::endObject() {
  raw("}");
  need_comma_ = true;
  return *this;
}

JsonWriter& JsonWriter::field(const char* k, const char* value) {
  key(k);
  escaped(value);
  return *this;
}

JsonWriter& JsonWriter::field(const char* k, double value, int decimals) {
  key(k);
  if (!std::isfinite(value)) {
    raw("null");
    return *this;
  }
  char tmp[40];
  std::snprintf(tmp, sizeof(tmp), "%.*f", decimals, value);
  raw(tmp);
  return *this;
}

JsonWriter& JsonWriter::field(const char* k, int64_t value) {
  key(k);
  char tmp[24];
  std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(value));
  raw(tmp);
  return *this;
}

JsonWriter& JsonWriter::fieldBool(const char* k, bool value) {
  key(k);
  raw(value ? "true" : "false");
  return *this;
}

JsonWriter& JsonWriter::fieldNull(const char* k) {
  key(k);
  raw("null");
  return *this;
}

}  // namespace st
