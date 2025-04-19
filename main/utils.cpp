#include "utils.h"
#include "sensors.h"
#include "motor.h"
#include "timekeeping.h"
#include "comms.h"

// NVS Storage for mode selection
Preferences preferences;

// Debug Interrupt Variables
const int debugButtonPin = 0;

// Error Handling
String errorMsg = "";

// Entering Debug Mode
void back_to_debug() {
  preferences.begin("esp32_config", false);
  preferences.putString("mode", "DEBUG");
  preferences.end();
  ESP.restart();
}

// Interrupts
void IRAM_ATTR handleDebugButton() {
  Serial.println("Debug button detected! Switching to debug mode...");
  back_to_debug();
}

void addError(const char* msg) {
  if (!errorMsg.isEmpty()) {
    errorMsg += " | "; // Separate multiple errors
  }
  errorMsg += msg;
}

void clearErrors() {
  errorMsg = "";
}

void go_to_sleep() {
  Serial.println("Entering deep sleep...");
  if (isDaylight) esp_sleep_enable_timer_wakeup(wakeupIntervalDaySec * 1000000);
  else esp_sleep_enable_timer_wakeup(wakeupIntervalNightSec * 1000000);
  esp_deep_sleep_start();
}

void mpu_setup() {
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void ads_setup() {
  ads.setGain(1);
}

void module_setup() {
  mpu_state = module_check(mpu, "MPU6050", 0x69);
  ina_state = module_check(ina, "INA219");
  rtc_state = module_check(rtc, "RTC");
  ads_state = module_check(ads, "ADS1115");
}

void setup_suite() {
  setup_motor();
  module_setup();
  if (mpu_state) mpu_setup();
  if (ads_state) ads_setup();
}

void process_routine() {
  preferences.begin("esp32_config", false);
  bool justDebugged = preferences.getBool("justDebugged", true);
  time_t currentTime = time(nullptr);
  if (justDebugged) {
    adjust_panel_tilt();
    preferences.putULong("lastMotorAdjust", currentTime);
    preferences.putBool("justDebugged", false);
  }
  else {
    lastMotorAdjust = preferences.getULong("lastMotorAdjust", currentTime);
    check_panel_interval(currentTime, lastMotorAdjust);
  }
  checkDaytime();
  if (WiFi.status() == WL_CONNECTED) publishData();
  Serial.print("Current ESP32 temperature: "); Serial.print(get_esp_temperature()); Serial.print(" C");
  Serial.println();
  Serial.println();
  preferences.end();
  go_to_sleep();
}