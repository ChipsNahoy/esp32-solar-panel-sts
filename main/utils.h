#pragma once

// ESP32 Libraries
#include "esp_sleep.h"
#include <time.h>
#include <Preferences.h>

extern Preferences preferences;
extern const int debugButtonPin;
extern String errorMsg;

void back_to_debug();
void addError(const char* msg);
void clearErrors();

template <typename T>
bool module_check(T &module, const String &name) {
  if (!module.begin()) {
    Serial.print(name); Serial.print(" not found.");
    addError((name + " not found").c_str());
    return false;
  }
  else {
    Serial.print(name);Serial.print(" ready.");
    return true;
    }
}

template <typename T>
bool module_check(T &module, const String &name, uint8_t address) {
  if (!module.begin(address)) {
    Serial.print(name); Serial.print(" ("); Serial.print(address); Serial.print(")"); Serial.print(" not found.");
    addError((name + "(" + address + ") not found").c_str());
    return false;
  }
  else {
    Serial.print(name); Serial.print(" ("); Serial.print(address); Serial.print(")"); Serial.print(" ready.");
    return true;
    }
}

void IRAM_ATTR handleDebugButton();
void go_to_sleep();
void mpu_setup();
void ads_setup();
void module_setup();
void setup_suite();
void process_routine();