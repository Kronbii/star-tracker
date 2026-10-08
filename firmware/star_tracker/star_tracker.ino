// Barn-door star tracker - ESP32 firmware.
//
// Hardware glue only: the planning, tracking maths, intervalometer and
// command language live in src/core/ (host-tested). This file wires them to
// the TMC2209, the home switch, the Canon remote optocouplers, Wi-Fi + web UI,
// the serial console and flash storage.
//
// Tasks:
//   motion task (core 1, high priority, 1 kHz): home switch -> controller,
//       intervalometer tick, refill the step queue 40 ms ahead, drive the
//       optocouplers.
//   Arduino loop (core 1, low priority): web server, serial console, button,
//       LED, battery, saving settings.
//   step ISR (20 kHz): executes queued step events (stepper_hal.cpp).
// A mutex guards the TrackerApp between the two tasks.

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "esp_timer.h"
#include "settings_store.h"
#include "src/core/app.h"
#include "src/core/st_math.h"
#include "stepper_hal.h"
#include "web_ui.h"

namespace {

st::TrackerApp g_app;
SemaphoreHandle_t g_lock = nullptr;
WebServer g_server(80);
char g_serial_line[128];
size_t g_serial_len = 0;

struct Lock {
  Lock() { xSemaphoreTake(g_lock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_lock); }
};

inline int64_t nowUs() { return esp_timer_get_time(); }

bool homeSwitchPressed() {
  // NC switch to GND with pull-up: LOW = closed = not pressed.
  return digitalRead(PIN_HOME_SWITCH) == HIGH;
}

void runCommand(const char* line, char* out, size_t n) {
  Lock l;
  g_app.handle(line, nowUs(), hal::executed(), out, n);
}

void motionTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    const int64_t now = nowUs();
    const bool sw = homeSwitchPressed();
    bool shutter, focus, active;
    {
      Lock l;
      st::MotionController& m = g_app.motion();
      m.updateSwitch(sw, now, hal::executed());
      g_app.tick(now);
      hal::fill(m, now, QUEUE_HORIZON_US);
      shutter = g_app.camera().shutterClosed();
      focus = g_app.camera().focusClosed();
      active = m.mode() != st::Mode::Idle || hal::queued() > 0;
    }
    digitalWrite(PIN_SHUTTER, shutter ? HIGH : LOW);
    digitalWrite(PIN_FOCUS, focus ? HIGH : LOW);
    if (DISABLE_DRIVER_WHEN_IDLE) hal::setEnabled(active);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
  }
}

void handleStatus() {
  static char buf[1024];
  {
    Lock l;
    g_app.statusJson(nowUs(), buf, sizeof(buf));
  }
  g_server.send(200, "application/json", buf);
}

void handleCmd() {
  static char buf[512];
  String c = g_server.arg("c");
  if (c.length() == 0) c = g_server.arg("plain");
  runCommand(c.c_str(), buf, sizeof(buf));
  g_server.send(200, "application/json", buf);
}

void startWifi() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "%s-%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
  const bool sta = STA_SSID[0] != '\0';
  WiFi.mode(sta ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(ssid, AP_PASSWORD);
  if (sta) WiFi.begin(STA_SSID, STA_PASSWORD);
  MDNS.begin(MDNS_NAME);
  Serial.printf("Wi-Fi AP '%s' -> http://%s or http://%s.local\n", ssid,
                WiFi.softAPIP().toString().c_str(), MDNS_NAME);
}

void pollSerial() {
  while (Serial.available()) {
    const char ch = char(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (g_serial_len == 0) continue;
      g_serial_line[g_serial_len] = '\0';
      static char out[1024];
      runCommand(g_serial_line, out, sizeof(out));
      Serial.println(out);
      g_serial_len = 0;
    } else if (g_serial_len + 1 < sizeof(g_serial_line)) {
      g_serial_line[g_serial_len++] = ch;
    }
  }
}

// BOOT button: short press toggles sidereal tracking, hold 2 s to home.
void pollButton() {
  static bool was_down = false;
  static uint32_t down_ms = 0;
  static bool long_done = false;
  const bool down = digitalRead(PIN_BUTTON) == LOW;
  const uint32_t ms = millis();
  if (down && !was_down) {
    down_ms = ms;
    long_done = false;
  }
  char out[256];
  if (down && !long_done && ms - down_ms > 2000) {
    long_done = true;
    runCommand("home", out, sizeof(out));
  }
  if (!down && was_down && !long_done && ms - down_ms > 40) {
    bool tracking;
    {
      Lock l;
      tracking = g_app.motion().mode() == st::Mode::Tracking;
    }
    runCommand(tracking ? "stop" : "track sidereal", out, sizeof(out));
  }
  was_down = down;
}

// Very short, dim blinks so the LED never shows up in a frame or spoils dark
// adaptation: 1/s tracking, 4/s moving, double blink on a fault.
void updateLed() {
  st::Mode mode;
  st::Fault fault;
  {
    Lock l;
    mode = g_app.motion().mode();
    fault = g_app.motion().fault();
  }
  const uint32_t ms = millis();
  bool on = false;
  if (fault != st::Fault::None && fault != st::Fault::EndOfTravel) {
    const uint32_t p = ms % 1500;
    on = p < 15 || (p > 200 && p < 215);
  } else if (mode == st::Mode::Tracking) {
    on = ms % 1000 < 10;
  } else if (mode != st::Mode::Idle) {
    on = ms % 250 < 10;
  }
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

void updateBattery() {
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  const float v = float(analogReadMilliVolts(PIN_VBAT)) * 1e-3f * VBAT_DIVIDER;
  Lock l;
  g_app.setBatteryVolts(v);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(PIN_HOME_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SHUTTER, OUTPUT);
  pinMode(PIN_FOCUS, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_SHUTTER, LOW);
  digitalWrite(PIN_FOCUS, LOW);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);

  st::Settings s;
  s.geom.microsteps = MICROSTEPS;
  const bool loaded = store::load(&s);
  s.geom.microsteps = MICROSTEPS;  // strapping on the driver wins
  g_lock = xSemaphoreCreateMutex();
  g_app.begin(s);
  Serial.printf("\nstar tracker: settings %s, R=%.2f mm c=%.2f mm lead=%.3f mm\n",
                loaded ? "loaded" : "default", s.geom.radius_mm, s.geom.contact_offset_mm,
                s.geom.lead_mm);

  hal::begin();
  xTaskCreatePinnedToCore(motionTask, "motion", 6144, nullptr, 5, nullptr, 1);

  startWifi();
  g_server.on("/", HTTP_GET, [] { g_server.send(200, "text/html", INDEX_HTML); });
  g_server.on("/api/status", HTTP_GET, handleStatus);
  g_server.on("/api/cmd", HTTP_ANY, handleCmd);
  g_server.onNotFound([] { g_server.send(404, "text/plain", "not found"); });
  g_server.begin();
  Serial.println(st::helpText());
}

void loop() {
  g_server.handleClient();
  pollSerial();
  pollButton();
  updateLed();
  updateBattery();
  bool save;
  st::Settings s;
  {
    Lock l;
    save = g_app.takeSaveRequest();
    if (save) s = g_app.settings();
  }
  if (save) {
    store::save(s);
    Serial.println("settings saved");
  }
  delay(2);
}
