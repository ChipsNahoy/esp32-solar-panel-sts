#pragma once

// RTC Library
#include "RTClib.h"

extern RTC_DS3231 rtc;
extern bool rtc_state;
extern bool isDaylight;
extern const int wakeupIntervalDaySec;
extern const int wakeupIntervalNightSec;
extern const int motorInterval;
extern unsigned long lastMotorAdjust;

bool should_sync_RTC();
void mark_RTC_synced();
bool sync_RTC_to_NTP();
void checkDaytime();
bool getCurrentTime(struct tm &out);
void check_panel_interval(time_t currentTime, unsigned long lastMotorAdjust);