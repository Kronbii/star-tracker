#include "stepper_hal.h"

#include <Arduino.h>

#include "config.h"
#include "esp_timer.h"
#include "soc/gpio_reg.h"
#include "src/core/step_queue.h"

static_assert(PIN_STEP < 32 && PIN_DIR < 32, "STEP/DIR must be GPIO0..31");

namespace {

st::StepQueue<512> g_queue;
volatile int32_t g_executed = 0;
volatile int8_t g_dir = 0;  // unknown at boot: first event sets DIR
volatile bool g_pulse_high = false;
volatile uint32_t g_late = 0;
hw_timer_t* g_timer = nullptr;

// Direct writes to the GPIO set/clear registers: a few cycles, ISR-safe.
inline void IRAM_ATTR writePin(int pin, bool high) {
  volatile uint32_t* reg =
      reinterpret_cast<volatile uint32_t*>(high ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG);
  *reg = 1u << pin;
}

// Integer-only ISR (no FPU use in interrupt context on the ESP32).
void ARDUINO_ISR_ATTR onStepTimer() {
  if (g_pulse_high) {
    writePin(PIN_STEP, false);  // pulse width = one timer period (50 us)
    g_pulse_high = false;
    return;
  }
  st::StepEvent e;
  if (!g_queue.peek(&e)) return;
  if (e.dir != g_dir) {
    // Change DIR one period before the step (TMC2209 setup time is 20 ns).
    writePin(PIN_DIR, (e.dir > 0) != INVERT_DIR);
    g_dir = e.dir;
    return;
  }
  const int64_t now = esp_timer_get_time();
  if (now < e.t_us) return;
  writePin(PIN_STEP, true);
  g_pulse_high = true;
  g_executed = g_executed + e.dir;
  if (now - e.t_us > 1000) g_late = g_late + 1;
  g_queue.pop();
}

}  // namespace

namespace hal {

void begin() {
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_DIR, LOW);
  setEnabled(true);
  g_timer = timerBegin(1000000);  // 1 MHz tick
  timerAttachInterrupt(g_timer, &onStepTimer);
  timerAlarm(g_timer, STEP_TIMER_PERIOD_US, true, 0);
}

void fill(st::MotionController& m, int64_t now_us, int64_t horizon_us) {
  while (!g_queue.full()) {
    const int64_t t = m.peekTime();
    if (t == st::MotionController::kNever || t > now_us + horizon_us) break;
    st::StepEvent e;
    m.next(&e);
    g_queue.push(e);
  }
}

int32_t executed() { return g_executed; }
uint32_t queued() { return g_queue.size(); }
uint32_t lateSteps() { return g_late; }

void setEnabled(bool on) { digitalWrite(PIN_ENABLE, on ? LOW : HIGH); }

}  // namespace hal
