#include <Arduino.h>
#include <BleCombo.h>
#include <EEPROM.h>

// --- Pins ---
const int btnUpPin = 13;
const int btnDownPin = 12;
const int btnNextPin = 14;
const int statusLED = 22;
const int battPin = 34;

// --- Voltage divider ---
const float R1 = 100000.0;
const float R2 = 100000.0;
const float VREF = 3.3;
const float CALIBRATION_FACTOR = 1.057;

// --- EEPROM addresses ---
const int EEPROM_VMIN_ADDR = 0;
const int EEPROM_VMAX_ADDR = 4;
const int EEPROM_LAST_PERCENT_ADDR = 8;

// --- Sampling ---
const int sampleCount = 12; // 12×5s = 1min
float voltageSamples[sampleCount];
int sampleIndex = 0;
bool samplesFilled = false;
bool reportedInitial = false;
unsigned long lastBatteryReport = 0;

// --- Learned min/max voltage ---
float v_min = 3.3;
float v_max = 4.26;

// --- Battery curve ---
const int N = 23;
const float cellVoltages[N] = {
  3.27, 3.61, 3.69, 3.71, 3.73,
  3.75, 3.77, 3.79, 3.80, 3.82,
  3.84, 3.85, 3.87, 3.91, 3.95,
  3.98, 4.02, 4.08, 4.11, 4.15,
  4.20, 4.23, 4.26
};
const float pct[N] = {
   0,   5,  10,  15,  20,
  25,  30,  35,  40,  45,
  50,  55,  60,  65,  70,
  75,  80,  85,  90,  95,
  98,  99, 100
};

// --- Button state ---
unsigned long lastScrollTime = 0;
const unsigned long scrollInterval = 150;
const unsigned long holdDelay = 100;
const unsigned long doubleClickWindow = 600;
unsigned long lastUpPress = 0;
unsigned long lastDownPress = 0;
bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastNext = HIGH;

void saveFloat(int addr, float val) {
  EEPROM.put(addr, val);
  EEPROM.commit();
}

float readFloat(int addr) {
  float val;
  EEPROM.get(addr, val);
  return val;
}

float getAveragedVoltage(int samples = 20) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(battPin);
    delay(2);
  }
  float avgRaw = sum / (float)samples;
  return avgRaw * (VREF / 4095.0) * (R1 + R2) / R2 * CALIBRATION_FACTOR;
}

float voltageToPercent(float v_cell) {
  if (v_cell <= cellVoltages[0]) return 0;
  if (v_cell >= cellVoltages[N - 1]) return 100;

  for (int i = 0; i < N - 1; i++) {
    if (v_cell >= cellVoltages[i] && v_cell < cellVoltages[i + 1]) {
      float t = (v_cell - cellVoltages[i]) / (cellVoltages[i + 1] - cellVoltages[i]);
      return pct[i] + t * (pct[i + 1] - pct[i]);
    }
  }
  return 0;
}

float getBatteryPercent(float voltage) {
  if (voltage < v_min) {
    v_min = voltage;
    saveFloat(EEPROM_VMIN_ADDR, v_min);
  }
  if (voltage > v_max) {
    v_max = voltage;
    saveFloat(EEPROM_VMAX_ADDR, v_max);
  }

  float curvePct = voltageToPercent(voltage);
  float learnedPct = (voltage - v_min) / (v_max - v_min) * 100.0;
  if (learnedPct < 0) learnedPct = 0;
  if (learnedPct > 100) learnedPct = 100;

  return (curvePct * 0.7) + (learnedPct * 0.3);
}

void setup() {
  Serial.begin(115200);
  pinMode(btnUpPin, INPUT_PULLUP);
  pinMode(btnDownPin, INPUT_PULLUP);
  pinMode(btnNextPin, INPUT_PULLUP);
  pinMode(statusLED, OUTPUT);
  analogReadResolution(12);
  delay(1000);

  EEPROM.begin(16);
  v_min = readFloat(EEPROM_VMIN_ADDR);
  v_max = readFloat(EEPROM_VMAX_ADDR);
  if (isnan(v_min) || v_min < 3.0 || v_min > 4.0) v_min = 3.3;
  if (isnan(v_max) || v_max < 4.0 || v_max > 4.3) v_max = 4.26;

  int lastSavedPercent = readFloat(EEPROM_LAST_PERCENT_ADDR);
  if (lastSavedPercent >= 0 && lastSavedPercent <= 100) {
    Keyboard.setBatteryLevel(lastSavedPercent);
    Serial.printf("Boot Battery Level: %d%% (from EEPROM)\n", lastSavedPercent);
  }

  Keyboard.begin();
  Mouse.begin();
  delay(3000); // allow voltage to settle
}

void reportBattery() {
  float voltage = getAveragedVoltage();
  voltageSamples[sampleIndex] = voltage;
  sampleIndex++;

  if (sampleIndex >= sampleCount) {
    sampleIndex = 0;
    samplesFilled = true;
  }

  float percent = 0;

  if (!reportedInitial) {
    // First runtime update at 5s
    percent = getBatteryPercent(voltage);
    Serial.printf("First live reading: %.3f V = %.1f%%\n", voltage, percent);
    reportedInitial = true;
  } else if (samplesFilled) {
    float sum = 0;
    for (int i = 0; i < sampleCount; i++) sum += voltageSamples[i];
    float avgV = sum / sampleCount;
    percent = getBatteryPercent(avgV);
    Serial.printf("AVG Battery: %.3f V = %.1f%%\n", avgV, percent);
  } else {
    Serial.printf("Collecting samples... (%d/%d)\n", sampleIndex, sampleCount);
    return;
  }

  int rounded = round(percent);
  Keyboard.setBatteryLevel(rounded);
  saveFloat(EEPROM_LAST_PERCENT_ADDR, rounded);
}

void loop() {
  unsigned long now = millis();

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

  digitalWrite(statusLED, HIGH);

  if (now - lastBatteryReport > 5000) {
    reportBattery();
    lastBatteryReport = now;
  }

  // BUTTON INPUTS
  bool upNow = digitalRead(btnUpPin);
  bool downNow = digitalRead(btnDownPin);
  bool nextNow = digitalRead(btnNextPin);

  if (upNow == LOW && lastUp == HIGH) {
    if (now - lastUpPress <= doubleClickWindow) {
      Keyboard.write(KEY_RIGHT_ARROW);
      Serial.println("Double press UP → RIGHT ARROW");
      lastUpPress = 0;
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

  if (nextNow == LOW && lastNext == HIGH) {
    Keyboard.write(KEY_RIGHT_ARROW);
    Serial.println("NEXT → RIGHT ARROW");
  }
  lastNext = nextNow;

  delay(5);
}
