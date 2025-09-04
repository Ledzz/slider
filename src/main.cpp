#include <Arduino.h>

/*
  Minimal ESP32-C3 + A4988 control (no acceleration, just constant speed).
  Wire A4988:
    STEP -> STEP_PIN
    DIR  -> DIR_PIN
    EN   -> EN_PIN (optional, active LOW)
    MS1..MS3 as you like (hardware straps)
    VMOT + GND to motor supply; Vref adjusted properly!
*/




// const int PIN_ENABLE = 4;
// const int PIN_RESET = 10;
// const int PIN_STEP = 7;
// const int PIN_DIR = 6;
// const int PIN_SLEEP = 21;
// const int PIN_MODE1 = 2;  // MS1
// const int PIN_MODE2 = 20;  // MS2
// const int PIN_MODE3 = 5;  // MS3


constexpr int STEP_PIN = 7;     // choose any output-capable GPIO
constexpr int DIR_PIN  = 6;
constexpr int EN_PIN   = 4;     // optional; tie low to enable if you prefer

// LEDC config
constexpr int LEDC_CHANNEL = 0; // 0..7 on ESP32-C3
constexpr int LEDC_TIMER   = 0; // 0..3 on ESP32-C3
constexpr int LEDC_RES     = 8; // bits (8 is plenty for 50% duty)
constexpr uint32_t DEFAULT_HZ = 1000; // 1 kHz = 1000 steps/s

// Track whether we're running
bool running = false;

void setDirection(bool forward) {
    digitalWrite(DIR_PIN, forward ? HIGH : LOW);
}

void setSpeedHz(uint32_t hz) {
    // Limit to something sane; A4988 can handle high rates but wiring matters.
    if (hz < 1) hz = 1;
    ledcWriteTone(LEDC_CHANNEL, hz); // Arduino helper sets frequency & 50% duty with ledcAttachPin()
}

void start() {
    if (running) return;
    // Attach STEP pin to LEDC channel; initial 50% duty via ledcWriteTone()
    ledcAttachPin(STEP_PIN, LEDC_CHANNEL);
    ledcSetup(LEDC_CHANNEL, DEFAULT_HZ, LEDC_RES);
    ledcWrite(LEDC_CHANNEL, (1 << (LEDC_RES - 1))); // 50% duty
    running = true;
}

void stop() {
    if (!running) return;
    ledcDetachPin(STEP_PIN);   // stop PWM cleanly
    digitalWrite(STEP_PIN, LOW);
    running = false;
}

void setup() {
    Serial.begin(115200);

    pinMode(DIR_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);

    // Enable the driver (LOW = enabled on A4988)
    digitalWrite(EN_PIN, LOW);

    setDirection(true);
    start();
    setSpeedHz(DEFAULT_HZ);
}

void loop() {
    delay(1000);
    setSpeedHz(1000);
    setDirection(false);
    delay(1000);
    setSpeedHz(1000);
    setDirection(true);
}