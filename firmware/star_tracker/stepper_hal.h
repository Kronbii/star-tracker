// Step executor: a 20 kHz hardware-timer ISR that plays queued StepEvents.
#pragma once

#include <stdint.h>

#include "src/core/motion.h"

namespace hal {

void begin();
// Moves events from the controller into the ISR queue, up to `horizon_us` ahead.
void fill(st::MotionController& m, int64_t now_us, int64_t horizon_us);
// Executed step counter (raw, same units as the controller's raw position).
int32_t executed();
uint32_t queued();
void setEnabled(bool on);
// Steps that were executed later than 1 ms after their scheduled time
// (diagnostic: should stay 0 while tracking).
uint32_t lateSteps();

}  // namespace hal
