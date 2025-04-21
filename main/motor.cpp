#include "motor.h"
#include "sensors.h"

#define RPWM 26
#define LPWM 25
#define EN   13
#define TOLERANCE 0.05
#define PWM_SPEED 255
#define MIN_PWM 60
#define KP 150
#define ANGLE_THRESH 0.5   // degrees: “movement” threshold
#define SAMPLE_DELAY 500   // ms between angle reads

void setup_motor() {
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(EN, OUTPUT);
  digitalWrite(EN, LOW);
}

void adjust_panel_tilt() {
  while (true) {
    uv_readings uv = read_uv();
    float diff = uv.leftUV - uv.rightUV;
    Serial.print("Diff (L-R): "); Serial.println(diff);
    if (abs(diff) > TOLERANCE) {
      digitalWrite(EN, HIGH);
      int pwm = min(int(abs(diff) * KP), 255);
      pwm = constrain(pwm, MIN_PWM, 255);

      if (diff > 0) {
        analogWrite(RPWM, pwm);
        analogWrite(LPWM, 0);
      } else {
        analogWrite(RPWM, 0);
        analogWrite(LPWM, pwm);
      }
      delay(100);
    } else {
      digitalWrite(EN, LOW);
      analogWrite(RPWM, 0);
      analogWrite(LPWM, 0);
      Serial.println("Panel balanced");
      break;
    }
  }
}

void moveToLowestPosition() {
  digitalWrite(EN, HIGH);
  analogWrite(RPWM, PWM_SPEED);
  analogWrite(LPWM, 0);

  // Take one initial angle reading
  float lastPitch = read_mpu().pitch;

  // Loop until movement under threshold
  while (true) {
    delay(SAMPLE_DELAY);
    float currentPitch = read_mpu().pitch;

    // if angle change is tiny → we’ve hit the stop
    if (fabs(currentPitch - lastPitch) < ANGLE_THRESH) {
      break;
    }
    lastPitch = currentPitch;
  }

  // cut power
  digitalWrite(EN, LOW);
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
}