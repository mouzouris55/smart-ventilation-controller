#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <RotaryEncoder.h>

#define DHTPIN 4
#define DHTTYPE DHT11

#define OLED_SDA 21
#define OLED_SCL 27
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define ENCODER_CLK 18
#define ENCODER_DT  19
#define ENCODER_SW  17

#define FAN_PWM_PIN 22
#define FAN_TACHO_PIN 23

enum Mode { AUTO_MODE, MANUAL_MODE };
Mode currentMode = AUTO_MODE;

float temperature = 0.0;
float humidity = 0.0;
int fanSpeedPercent = 0;
volatile unsigned int tachoPulses = 0;
int rpm = 0;

unsigned long lastDHTRead = 0;
unsigned long lastAutoCheck = 0;
unsigned long lastRPMCalc = 0;
unsigned long lastTachoPulseTime = 0;
unsigned long lastInputDisplay = 0;

// Μεταβλητές Encoder & Button
RotaryEncoder encoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::TWO03);
long lastEncoderPosition = 0;

bool buttonRawState = HIGH;
bool buttonStableState = HIGH;
unsigned long buttonLastChange = 0;

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Η διακοπή μετρά κάθε παλμό του ανεμιστήρα χωρίς να σταματά το loop.
void IRAM_ATTR tachoISR() {
  unsigned long currentTime = micros();
  if (currentTime - lastTachoPulseTime > 2000) {
    tachoPulses++;
    lastTachoPulseTime = currentTime;
  }
}

// Μετατρέπει το ποσοστό ταχύτητας σε τιμή PWM από 0 έως 255.
void setFanSpeed(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  fanSpeedPercent = percent;
  int dutyCycle = map(percent, 0, 100, 0, 255);
  ledcWrite(0, dutyCycle);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("T:%.1fC  H:%.1f%%", temperature, humidity);
  display.setCursor(0, 12);
  // S=0 while the encoder button is held down. This makes a wiring problem
  // visible directly on the OLED, without requiring the Serial Monitor.
  display.printf("M:%s S:%d", currentMode == AUTO_MODE ? "AUTO" : "MANUAL",
                 digitalRead(ENCODER_SW));
  display.setCursor(0, 24);
  display.printf("Fan: %d%%  RPM: %d", fanSpeedPercent, rpm);
  display.display();
}

// Ο DHT11 διαβάζεται κάθε 2 δευτερόλεπτα.
void readSensor(unsigned long currentMillis) {
  if (currentMillis - lastDHTRead < 2000) return;

  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (!isnan(newHumidity) && !isnan(newTemperature)) {
    humidity = newHumidity;
    temperature = newTemperature;
  }

  lastDHTRead = currentMillis;
}

// Μετρά τους παλμούς του τελευταίου δευτερολέπτου και τους μετατρέπει σε RPM.
void calculateRPM(unsigned long currentMillis) {
  if (currentMillis - lastRPMCalc < 1000) return;

  noInterrupts();
  unsigned int pulses = tachoPulses;
  tachoPulses = 0;
  interrupts();

  rpm = (pulses / 2) * 60;
  lastRPMCalc = currentMillis;
  updateDisplay();
}

// Περιμένει 30 ms πριν δεχτεί αλλαγή, ώστε να αγνοεί το bouncing του κουμπιού.
void readButton(unsigned long currentMillis) {
  bool buttonState = digitalRead(ENCODER_SW);

  if (buttonState != buttonRawState) {
    buttonRawState = buttonState;
    buttonLastChange = currentMillis;
  }

  if (currentMillis - buttonLastChange >= 30 &&
      buttonStableState != buttonRawState) {
    buttonStableState = buttonRawState;

    if (buttonStableState == LOW) {
      currentMode = (currentMode == AUTO_MODE) ? MANUAL_MODE : AUTO_MODE;
      lastEncoderPosition = encoder.getPosition();
      updateDisplay();
    }
  }
}

// Στην αυτόματη λειτουργία, η υγρασία καθορίζει την ταχύτητα του ανεμιστήρα.
void updateAutomaticFan(unsigned long currentMillis) {
  if (currentMillis - lastAutoCheck < 10000) return;

  if (humidity < 55) {
    setFanSpeed(0);
  } else if (humidity < 60) {
    setFanSpeed(25);
  } else if (humidity < 75) {
    setFanSpeed(60);
  } else {
    setFanSpeed(100);
  }

  lastAutoCheck = currentMillis;
  updateDisplay();
}

// Στη χειροκίνητη λειτουργία, κάθε βήμα του encoder αλλάζει την ταχύτητα κατά 5%.
void updateManualFan() {
  long position = encoder.getPosition();
  long steps = position - lastEncoderPosition;
  lastEncoderPosition = position;

  if (steps != 0) {
    setFanSpeed(fanSpeedPercent + steps * 5);
    updateDisplay();
  }
}

void refreshDisplay(unsigned long currentMillis) {
  if (currentMillis - lastInputDisplay >= 150) {
    updateDisplay();
    lastInputDisplay = currentMillis;
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
  }
  display.clearDisplay();
  display.display();

  dht.begin();

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  buttonRawState = digitalRead(ENCODER_SW);
  buttonStableState = buttonRawState;
  lastEncoderPosition = encoder.getPosition();
  Serial.printf("Encoder ready: CLK=%d, DT=%d, SW=%d (initial SW=%d)\n",
                ENCODER_CLK, ENCODER_DT, ENCODER_SW, buttonStableState);

  ledcSetup(0, 25000, 8);
  ledcAttachPin(FAN_PWM_PIN, 0);
  setFanSpeed(0);

  pinMode(FAN_TACHO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACHO_PIN), tachoISR, FALLING);
}

void loop() {
  unsigned long currentMillis = millis();
  encoder.tick();

  // Κάθε συνάρτηση ελέγχει μόνη της πότε πρέπει να εκτελεστεί.
  readButton(currentMillis);
  readSensor(currentMillis);
  calculateRPM(currentMillis);
  refreshDisplay(currentMillis);

  if (currentMode == AUTO_MODE) {
    updateAutomaticFan(currentMillis);
  } else {
    updateManualFan();
  }
}
