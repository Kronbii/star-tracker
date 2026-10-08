// Bounded JSON object writer (no allocation). Truncates safely if the buffer
// is too small; `ok()` reports whether everything fitted.
#pragma once

#include <cstddef>
#include <cstdint>

namespace st {

class JsonWriter {
 public:
  JsonWriter(char* buf, size_t cap);
  JsonWriter& beginObject(const char* key = nullptr);
  JsonWriter& endObject();
  JsonWriter& field(const char* key, const char* value);
  JsonWriter& field(const char* key, double value, int decimals = 3);
  JsonWriter& field(const char* key, int64_t value);
  JsonWriter& fieldBool(const char* key, bool value);
  JsonWriter& fieldNull(const char* key);
  bool ok() const { return ok_; }
  const char* c_str() const { return buf_; }
  size_t length() const { return len_; }

 private:
  void raw(const char* s);
  void key(const char* k);
  void escaped(const char* s);
  char* buf_;
  size_t cap_;
  size_t len_ = 0;
  bool ok_ = true;
  bool need_comma_ = false;
};

}  // namespace st
