#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Stepper.h>
#include <ESP32Servo.h>

#define EW1 25
#define EW2 26
#define EW3 27
#define EW4 14

#define SN1 21
#define SN2 22
#define SN3 19
#define SN4 18

#define SERVO_PIN 32
#define PUMP_PIN 33
#define VOLT_PIN 34
#define WIFI_LED 2

#define STEPS_PER_REV 2048

Stepper motorSN(STEPS_PER_REV, SN1, SN3, SN2, SN4);
Stepper motorEW(STEPS_PER_REV, EW1, EW3, EW2, EW4);

Servo cleanServo;

const char* ssid = "Pf";
const char* password = "123456789";

String targetURL = "http://10.202.210.113:5000/get_target";
String doneURL = "http://10.202.210.113:5000/motor_done";
String sensorURL = "http://10.202.210.113:5000/esp";

int currentSN = 0;
int currentEW = 0;

unsigned long lastSensor = 0;

bool cleaningDone = false;

void setup() {

  Serial.begin(115200);

  pinMode(WIFI_LED, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  motorSN.setSpeed(8);
  motorEW.setSpeed(8);

  cleanServo.attach(SERVO_PIN);

  connectWiFi();
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi lost. Reconnecting...");
    digitalWrite(WIFI_LED, LOW);

    connectWiFi();
    return;
  }

  getTarget();

  if (millis() - lastSensor > 10000) {

    sendSensor();
    lastSensor = millis();
  }

  delay(2000);
}

void connectWiFi() {

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    digitalWrite(WIFI_LED, HIGH);
    delay(300);

    digitalWrite(WIFI_LED, LOW);
    delay(300);

    Serial.print(".");
  }

  digitalWrite(WIFI_LED, HIGH);

  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());
}

void getTarget() {

  HTTPClient http;

  http.begin(targetURL);

  int code = http.GET();

  if (code == 200) {

    StaticJsonDocument<300> doc;

    DeserializationError error = deserializeJson(doc, http.getString());

    if (error) {
      Serial.println("JSON parse failed");
      http.end();
      return;
    }

    int targetSN = doc["sn"];
    int targetEW = doc["ew"];

    targetSN = constrain(targetSN, -45, 45);
    targetEW = constrain(targetEW, -45, 45);

    bool cleaning = doc["cleaning"];
    bool pump = doc["pump"];

    if (targetSN != currentSN)
      moveMotorSN(targetSN);

    if (targetEW != currentEW)
      moveMotorEW(targetEW);

    if (cleaning && !cleaningDone) {
      cleanPanel();
      cleaningDone = true;
    }

    if (!cleaning)
      cleaningDone = false;

    digitalWrite(PUMP_PIN, pump);

    sendDone();
  }

  http.end();
}

void moveMotorSN(int target) {

  int diff = target - currentSN;

  if (diff == 0) return;

  int steps = (STEPS_PER_REV * diff) / 360;

  motorSN.step(steps);

  currentSN = target;

  Serial.print("SN=");
  Serial.println(currentSN);
}

void moveMotorEW(int target) {

  int diff = target - currentEW;

  if (diff == 0) return;

  int steps = (STEPS_PER_REV * diff) / 360;

  motorEW.step(steps);

  currentEW = target;

  Serial.print("EW=");
  Serial.println(currentEW);
}

void sendDone() {

  HTTPClient http;

  http.begin(doneURL);

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;

  doc["sn"] = currentSN;
  doc["ew"] = currentEW;

  String body;

  serializeJson(doc, body);

  http.POST(body);

  http.end();
}

void sendSensor() {

  float voltage = readVoltage();

  Serial.print("Voltage: ");
  Serial.println(voltage);

  HTTPClient http;

  http.begin(sensorURL);

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;

  doc["voltage"] = voltage;
  doc["temp"] = 30;
  doc["gas"] = 0;

  String body;

  serializeJson(doc, body);

  http.POST(body);

  http.end();
}

float readVoltage() {

  long sum = 0;

  for (int i = 0; i < 20; i++) {
    sum += analogRead(VOLT_PIN);
    delay(5);
  }

  float adc = sum / 20.0;

  float adcVoltage = adc * (3.3 / 4095.0);

  return adcVoltage * 12.0;
}

void cleanPanel() {

  Serial.println("Cleaning panel");

  cleanServo.write(0);
  delay(2000);

  cleanServo.write(90);
  delay(2000);

  cleanServo.write(0);
}