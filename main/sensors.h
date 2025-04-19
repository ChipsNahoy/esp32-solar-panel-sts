#pragma once

// ADC Library
#include "ADS1X15.h"

// Accelerometer Libraries
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Voltage/Current Sensor Library
#include <Adafruit_INA219.h>

extern ADS1115 ads;
extern Adafruit_MPU6050 mpu;
extern Adafruit_INA219 ina;

extern bool ads_state;
extern bool mpu_state;
extern bool ina_state;

struct uv_readings {
  float leftUV, rightUV;
};

struct sensor_data {
  float l_uv, r_uv, v1, i1, v2, i2, roll, pitch, espTemperature;
  char timestamp[20];
};

// ESP32 Internal Temperature Sensor
float get_esp_temperature();

uv_readings read_uv();
float get_load_current();
float get_load_voltage();
float uv_bit_to_mW(int16_t raw);
float getUV(int adc_pin);
sensor_data prepData();
