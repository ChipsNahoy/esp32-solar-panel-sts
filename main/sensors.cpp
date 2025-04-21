#include "sensors.h"
#include "timekeeping.h"

extern "C" uint8_t temprature_sens_read();

// ADC Variables
ADS1115 ads(0x48);
bool ads_state = false;

// UV Sensor Variables
#define l_uv_adc_pin 0
#define r_uv_adc_pin 1

// Accelerometer Variables
Adafruit_MPU6050 mpu;
bool mpu_state = false;

// Voltage/Current Sensor Variables
#define acs_adc_pin 2
#define volt_divide_adc_pin 3
Adafruit_INA219 ina(0x41);
bool ina_state = false;

uv_readings read_uv() {
  const int N = 7;                     // must be odd
  float left[N], right[N];
  
  // 1) Gather N samples
  for (int i = 0; i < N; i++) {
    left[i]  = getUV(l_uv_adc_pin);
    right[i] = getUV(r_uv_adc_pin);
    delay(10);                         // give ADS time to settle
  }

  // 2) Sort both arrays
  std::sort(left,  left + N);
  std::sort(right, right + N);

  // 3) Sum the “middle” values (index 1 through N-2), discarding min & max
  float ls = 0, rs = 0;
  for (int i = 1; i < N - 1; i++) {
    ls += left[i];
    rs += right[i];
  }

  // 4) Compute final average
  uv_readings r;
  r.leftUV  = ls / (N - 2);
  r.rightUV = rs / (N - 2);
  return r;
}

mpu_readings read_mpu() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  mpu_readings readings;
  readings.roll  = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;
  readings.pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  return readings;
}

// Supporting Functions
float get_load_current() {
  int16_t iRaw = ads.readADC(acs_adc_pin);
  float voltage = iRaw * 0.125 / 1000.0;
  return ((voltage - 2.5)/0.04) * 1000.00;
}

float get_load_voltage() {
  int16_t iRaw = ads.readADC(volt_divide_adc_pin);
  float voltage = iRaw * 0.125 / 1000.0;
  return voltage * (10.0 + 4.7) / 4.7;
}

float uv_bit_to_mW(int16_t raw) {
  float voltage = raw * 0.125 / 1000.0;
  return (voltage - 1.0) * (15.0 / 1.8);
}

float getUV(int adc_pin) {
  int16_t uvRaw = ads.readADC(adc_pin);
  return uv_bit_to_mW(uvRaw);
}

float get_esp_temperature() {
  return (temprature_sens_read() - 32) / 1.8; // Convert Fahrenheit to Celsius
}

sensor_data prepData() {
  sensor_data datas;
  if (mpu_state) {
    mpu_readings mpu_reads = read_mpu();
    datas.roll  = mpu_reads.roll;
    datas.pitch = mpu_reads.pitch;
    
  }
  else {
    datas.roll  = -99;
    datas.pitch = -99;
  }
  
  if (ads_state) {
    uv_readings uv = read_uv();
    datas.l_uv  = uv.leftUV;
    datas.r_uv  = uv.rightUV;
    datas.v1    = get_load_voltage();
    datas.i1    = get_load_current();
  }
  else {
    datas.l_uv  = -99;
    datas.r_uv  = -99;
    datas.v1    = -99;
    datas.i1    = -99;
  }
  
  if (ina_state) {
    datas.v2    = abs(ina.getBusVoltage_V());
    datas.i2    = abs(ina.getCurrent_mA());
  }
  else {
    datas.v2    = -99;
    datas.i2    = -99;
  }

  datas.espTemperature = get_esp_temperature();
  struct tm tm;
  if (rtc_state) {
    DateTime now = rtc.now();
    tm.tm_year = now.year() - 1900;
    tm.tm_mon  = now.month() - 1;
    tm.tm_mday = now.day();
    tm.tm_hour = now.hour();
    tm.tm_min  = now.minute();
    tm.tm_sec  = now.second();
    datas.espTemperature = rtc.getTemperature();
  } else if (!getLocalTime(&tm)) {
    memset(&tm, 0, sizeof(tm));
  }
  strftime(datas.datetime, sizeof(datas.datetime), "%Y-%m-%dT%H:%M:%SZ", &tm);
  
  return datas;
}
