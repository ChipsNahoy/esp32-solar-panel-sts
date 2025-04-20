#include "timekeeping.h"
#include "utils.h"
#include "motor.h"

// RTC Variables
RTC_DS3231 rtc;
bool rtc_state = false;
const long seconds_before_rtc_sync = 24 * 3600; // Weekly(604800) Hourly(3600) 

// Timekeeping Variables
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "asia.pool.ntp.org";
const char* ntpServer3 = "time.google.com";
const long  gmtOffset_sec = 7 * 3600; // Adjust for your timezone (e.g., GMT+7)
const int   daylightOffset_sec = 0;  // No daylight saving
bool isDaylight = true;
const int wakeupIntervalDaySec = 60;
const int wakeupIntervalNightSec = 15 * 60;
const int motorInterval = 5 * 60;
unsigned long lastMotorAdjust;

bool should_sync_RTC() {
  preferences.begin("rtc_sync", false);
  time_t lastSync = preferences.getULong("lastSync", 0);
  preferences.end();

  return (rtc.now().unixtime() - lastSync) >= seconds_before_rtc_sync;
}

void mark_RTC_synced() {
  preferences.begin("rtc_sync", false);
  preferences.putULong("lastSync", rtc.now().unixtime());
  preferences.end();
}

bool sync_RTC_to_NTP() {
  if (!rtc_state) return false;
  preferences.begin("esp32_config", false);
  bool justDebugged = preferences.getBool("justDebugged", true);
  if (!should_sync_RTC() && !justDebugged) {
    Serial.println("RTC sync not needed yet.");
    return true;
  }

  int retry_attempt = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
  Serial.println("Waiting for time sync...");

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    if (retry_attempt > 5) {
      Serial.println("Failed 5 attempts to obtain time. Will use existing time measurement.");
      addError("NTP sync failed");
      return false;
    }
    Serial.println("Failed to obtain time. Retrying...");
    delay(500);
    retry_attempt += 1;
  }

  rtc.adjust(DateTime(
  timeinfo.tm_year + 1900,
  timeinfo.tm_mon  + 1,
  timeinfo.tm_mday,
  timeinfo.tm_hour,
  timeinfo.tm_min,
  timeinfo.tm_sec
  ));

  mark_RTC_synced();
  Serial.println("Time synced!");
  return true;
}

bool getCurrentTime(struct tm &out) {
  if (rtc_state) {
    DateTime dt = rtc.now();
    out.tm_year = dt.year() - 1900;
    out.tm_mon  = dt.month() - 1;
    out.tm_mday = dt.day();
    out.tm_hour = dt.hour();
    out.tm_min  = dt.minute();
    out.tm_sec  = dt.second();
    mktime(&out);  
    return true;
  }

  // fallback to internal RTC / NTP
  return getLocalTime(&out);
}

void check_panel_interval(time_t currentTime, unsigned long lastMotorAdjust) {
  if ((currentTime - lastMotorAdjust) >= (motorInterval)) {
    Serial.println("Performing Motor Adjustment");
    adjust_panel_tilt();
    preferences.begin("esp32_config", false);
    preferences.putULong("lastMotorAdjust", currentTime);
    preferences.end();
  }
}

void checkDaytime() {
  struct tm timeinfo;
  if (!getCurrentTime(timeinfo)) {
    Serial.println("Failed to obtain time, trying to sync with NTP...");
    sync_RTC_to_NTP();
    if (!getCurrentTime(timeinfo)) {
      addError("Time read failed");
      return;
    }
  }
  int currentHour = timeinfo.tm_hour;
  if (currentHour == 4 && !isDaylight) {
    isDaylight = true;
    Serial.println("Switching to Daylight Mode");
  }
  if (currentHour == 18 && isDaylight) {
    isDaylight = false;
    Serial.println("Switching to Night Mode");
    moveToLowestPosition();  // Move actuator to lowest position
  }
}