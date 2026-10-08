// Settings persistence in NVS flash (Preferences).
#pragma once

#include "src/core/app.h"

namespace store {

// Loads saved settings over the defaults in `s`. Returns false if none saved.
bool load(st::Settings* s);
void save(const st::Settings& s);

}  // namespace store
