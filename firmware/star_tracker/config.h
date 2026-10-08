// Build-time configuration: pins, Wi-Fi, defaults.
// Target: ESP32-WROOM-32 DevKitC (esp32:esp32:esp32). See hardware/wiring.md.
#pragma once

// ---- Stepper driver (TMC2209 in standalone STEP/DIR mode) -------------------
// MS1 = MS2 = 3V3 on the driver board -> 1/16 microsteps (interpolated to 1/256).
constexpr int PIN_STEP = 26;   // must be < 32 (direct register writes in the ISR)
constexpr int PIN_DIR = 27;    // must be < 32
constexpr int PIN_ENABLE = 25; // TMC2209 EN, active low
constexpr bool INVERT_DIR = false;  // flip if "track" closes the boards
constexpr int MICROSTEPS = 16;      // must match MS1/MS2 strapping

// ---- Inputs -----------------------------------------------------------------
// Home switch: normally-closed micro switch between the pin and GND.
// Closed (not pressed) reads LOW; pressed or a broken wire reads HIGH, which the
// firmware treats as "at home" - fail-safe.
constexpr int PIN_HOME_SWITCH = 33;
constexpr int PIN_BUTTON = 0;       // on-board BOOT button: short = track/stop, long = home
constexpr int PIN_VBAT = 34;        // 12 V via 100k / 22k divider (ADC1)
constexpr float VBAT_DIVIDER = (100.0f + 22.0f) / 22.0f;

// ---- Outputs ----------------------------------------------------------------
// Optocoupler LEDs (PC817) via 470R; the transistors short the Canon E3 lines.
constexpr int PIN_SHUTTER = 18;     // -> opto 1 -> tip (shutter)
constexpr int PIN_FOCUS = 19;       // -> opto 2 -> ring (focus)
constexpr int PIN_LED = 2;          // on-board LED: brief, dim blinks only

// ---- Wi-Fi --------------------------------------------------------------------
// The tracker always runs its own access point; open http://192.168.4.1 or
// http://startracker.local. Optionally also join a home network (leave empty).
constexpr const char* AP_SSID_PREFIX = "StarTracker";
constexpr const char* AP_PASSWORD = "orion-m42";  // >= 8 chars; change it
constexpr const char* STA_SSID = "";
constexpr const char* STA_PASSWORD = "";
constexpr const char* MDNS_NAME = "startracker";

// ---- Timing -------------------------------------------------------------------
constexpr int STEP_TIMER_PERIOD_US = 50;  // 20 kHz ISR -> max 10 kHz step rate
constexpr int64_t QUEUE_HORIZON_US = 40000;
constexpr bool DISABLE_DRIVER_WHEN_IDLE = false;  // Tr8x2 is self-locking
