#include <Arduino.h>
#include <BleCombo.h>

const int btnScrollUp = 13;
const int btnScrollDown = 12;
const int btnNext = 14;
const int statusLED = 2;

const int battPin = 34;       // ADC pin for battery voltage
const float voltageDividerRatio = 2.0; // Adjust based on your resistors

// Timing configs
const unsigned long scrollInterval = 150;
const unsigned long doublePressMaxDelay = 600;
const unsigned long holdScrollRepeatDelay = 100;
const unsigned long batteryUpdateInterval = 5000;

unsigned long lastActionTime = 0;
unsigned long lastUpPress = 0;
unsigned long lastDownPress = 0;
unsigned long lastBatteryUpdate = 0;

bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastRight = HIGH;

bool upHeld = false;
bool downHeld = false;
unsigned long upHoldStart = 0;
unsigned long downHoldStart = 0;

void setup() {
  Serial.begin(115200);

  pinMode(btnScrollUp, INPUT_PULLUP);
  pinMode(btnScrollDown, INPUT_PULLUP);
  pinMode(btnNext, INPUT_PULLUP);
  pinMode(statusLED, OUTPUT);

  analogReadResolution(12); // 12-bit ADC for ESP32

  delay(1000);
  Keyboard.begin();
  Mouse.begin();
}

void updateBattery() {
  int raw = analogRead(battPin);
  float voltage = raw * (3.3 / 4095.0) * voltageDividerRatio;

  // Map voltage (example: 3.0V = 0%, 4.2V = 100%)
  int percent = (int)((voltage - 3.0) / (4.2 - 3.0) * 100);
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  Serial.printf("Battery voltage: %.2f V, level: %d%%\n", voltage, percent);

  Keyboard.setBatteryLevel(percent);
}

void loop() {
  static unsigned long lastLED = 0;
  static bool ledOn = false;
  unsigned long now = millis();

  // LED status blinking if disconnected
  if (!Keyboard.isConnected()) {
    if (now - lastLED > 200) {
      ledOn = !ledOn;
      digitalWrite(statusLED, ledOn ? HIGH : LOW);
      lastLED = now;
    }
    return;
  }
  digitalWrite(statusLED, HIGH);

  // Update battery level every 5 seconds
  if (now - lastBatteryUpdate > batteryUpdateInterval) {
    updateBattery();
    lastBatteryUpdate = now;
  }

  // -------- UP button --------
  bool nowUp = digitalRead(btnScrollUp);
  if (nowUp == LOW) {
    if (!upHeld && lastUp == HIGH) {
      if (now - lastUpPress < doublePressMaxDelay) {
        Serial.println("Double press UP => RIGHT ARROW");
        Keyboard.write(KEY_RIGHT_ARROW);
        lastUpPress = 0;
      } else {
        upHoldStart = now;
        lastUpPress = now;
      }
    }
    upHeld = true;
  } else {
    upHeld = false;
  }

  if (upHeld && now - upHoldStart > holdScrollRepeatDelay && now - lastActionTime > scrollInterval) {
    Mouse.move(0, 0, 1); // scroll up
    Serial.println("Hold scroll UP");
    lastActionTime = now;
  }
  lastUp = nowUp;

  // -------- DOWN button --------
  bool nowDown = digitalRead(btnScrollDown);
  if (nowDown == LOW) {
    if (!downHeld && lastDown == HIGH) {
      if (now - lastDownPress < doublePressMaxDelay) {
        Serial.println("Double press DOWN => LEFT ARROW");
        Keyboard.write(KEY_LEFT_ARROW);
        lastDownPress = 0;
      } else {
        downHoldStart = now;
        lastDownPress = now;
      }
    }
    downHeld = true;
  } else {
    downHeld = false;
  }

  if (downHeld && now - downHoldStart > holdScrollRepeatDelay && now - lastActionTime > scrollInterval) {
    Mouse.move(0, 0, -1); // scroll down
    Serial.println("Hold scroll DOWN");
    lastActionTime = now;
  }
  lastDown = nowDown;

  // -------- RIGHT button --------
  bool nowRight = digitalRead(btnNext);
  if (nowRight == LOW && lastRight == HIGH && now - lastActionTime > scrollInterval) {
    Serial.println("RIGHT ARROW (Single press)");
    Keyboard.write(KEY_RIGHT_ARROW);
    lastActionTime = now;
  }
  lastRight = nowRight;

  delay(5);
}
