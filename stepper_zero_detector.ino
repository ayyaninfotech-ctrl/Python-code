#include <Stepper.h>

#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14

#define IR_PIN 34

#define STEPS_PER_REV 2048

Stepper motor(STEPS_PER_REV, IN1, IN3, IN2, IN4);

bool zeroFound = false;

void setup() {

  Serial.begin(115200);

  pinMode(IR_PIN, INPUT);

  motor.setSpeed(10);   // RPM

  Serial.println("Searching ZERO position...");

  // -------- ZERO SEARCH --------
  while (digitalRead(IR_PIN) != LOW) {

    motor.step(1);   // move step by step
  }

  zeroFound = true;
  Serial.println("ZERO POSITION FOUND");
}

void loop() {

  if (Serial.available()) {

    char cmd = Serial.read();

    if (cmd == '4') {

      Serial.println("Rotate +45 deg");
      motor.step(256);
    }

    else if (cmd == '6') {

      Serial.println("Rotate -45 deg");
      motor.step(-256);
    }

    else if (cmd == '8') {

      Serial.println("Rotate +90 deg");
      motor.step(512);
    }

    else if (cmd == '2') {

      Serial.println("Rotate -90 deg");
      motor.step(-512);
    }

    else if (cmd == '0') {

      Serial.println("Return to ZERO");

      while (digitalRead(IR_PIN) != LOW) {

        motor.step(-1);
      }

      Serial.println("ZERO reached");
    }
  }
}