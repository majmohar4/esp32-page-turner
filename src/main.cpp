#include <Arduino.h>
#include <BleCombo.h>

const int btnUpPin = 13;
const int btnDownPin = 12;
const int btnNextPin = 14;
const int statusLED = 22;  // Bluetooth status LED pin
const int battPin = 34;

const float voltageDividerRatio = 2.0; // if R1 = R2 = 100k

unsigned long lastScrollTime = 0;
const unsigned long scrollInterval = 150;
const unsigned long holdDelay = 100;
const unsigned long doubleClickWindow = 600;
const unsigned long batteryReportInterval = 5000;

unsigned long lastUpPress = 0;
unsigned long lastDownPress = 0;
unsigned long lastBatteryReport = 0;

bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastNext = HIGH;

void setup() {
  Serial.begin(115200);
  pinMode(btnUpPin, INPUT_PULLUP);
  pinMode(btnDownPin, INPUT_PULLUP);
  pinMode(btnNextPin, INPUT_PULLUP);
  pinMode(statusLED, OUTPUT);
  analogReadResolution(12);
  delay(1000);
  Keyboard.begin();
  Mouse.begin();
}

void reportBattery() {
  int raw = analogRead(battPin);
  float voltage = raw * (3.3 / 4095.0) * voltageDividerRatio;
  int percent = (int)((voltage - 3.0) / (4.2 - 3.0) * 100.0);
  percent = constrain(percent, 0, 100);
  Keyboard.setBatteryLevel(percent);
  Serial.printf("Battery: %.2fV (%d%%)\n", voltage, percent);
}

void loop() {
  unsigned long now = millis();

  // BLE not connected = blink LED
  if (!Keyboard.isConnected()) {
    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    if (now - lastBlink > 200) {
      ledOn = !ledOn;
      digitalWrite(statusLED, ledOn ? HIGH : LOW);
      lastBlink = now;
    }
    return;
  }

  // BLE connected = LED ON
  digitalWrite(statusLED, HIGH);

  if (now - lastBatteryReport > batteryReportInterval) {
    reportBattery();
    lastBatteryReport = now;
  }

  // Read button states
  bool upNow = digitalRead(btnUpPin);
  bool downNow = digitalRead(btnDownPin);
  bool nextNow = digitalRead(btnNextPin);

  // ----- SCROLL UP -----
  if (upNow == LOW && lastUp == HIGH) {
    if (now - lastUpPress <= doubleClickWindow) {
      Keyboard.write(KEY_RIGHT_ARROW);
      Serial.println("Double press UP → RIGHT ARROW");
      lastUpPress = 0;  // reset to avoid triple triggers
    } else {
      Mouse.move(0, 0, 1);
      lastUpPress = now;
      Serial.println("Scroll UP");
    }
  }
  if (upNow == LOW && now - lastScrollTime > holdDelay) {
    Mouse.move(0, 0, 1);
    lastScrollTime = now;
  }
  lastUp = upNow;

  // ----- SCROLL DOWN -----
  if (downNow == LOW && lastDown == HIGH) {
    if (now - lastDownPress <= doubleClickWindow) {
      Keyboard.write(KEY_LEFT_ARROW);
      Serial.println("Double press DOWN → LEFT ARROW");
      lastDownPress = 0;
    } else {
      Mouse.move(0, 0, -1);
      lastDownPress = now;
      Serial.println("Scroll DOWN");
    }
  }
  if (downNow == LOW && now - lastScrollTime > holdDelay) {
    Mouse.move(0, 0, -1);
    lastScrollTime = now;
  }
  lastDown = downNow;

  // ----- NEXT → Right arrow -----
  if (nextNow == LOW && lastNext == HIGH) {
    Keyboard.write(KEY_RIGHT_ARROW);
    Serial.println("NEXT button → RIGHT ARROW");
  }
  lastNext = nextNow;

  delay(5);
}
