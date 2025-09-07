#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

// Pin definitions
constexpr int STEP_PIN = 7;
constexpr int DIR_PIN  = 6;
constexpr int EN_PIN   = 4;
constexpr int ENDSTOP_LEFT_PIN  = 2;  // Left endstop (choose available GPIO)
constexpr int ENDSTOP_RIGHT_PIN = 1;  // Right endstop (choose available GPIO)

// LEDC config
constexpr int LEDC_CHANNEL = 0;
constexpr int LEDC_TIMER   = 0;
constexpr int LEDC_RES     = 8;

// Speed and acceleration parameters
constexpr uint32_t MIN_SPEED_HZ = 100;     // Minimum speed (steps/sec)
constexpr uint32_t MAX_SPEED_HZ = 2000;    // Maximum speed (steps/sec)
constexpr uint32_t ACCEL_STEPS_PER_SEC2 = 500; // Acceleration (steps/sec²)
constexpr uint32_t RAMP_UPDATE_INTERVAL_MS = 20; // How often to update speed during ramp

// Motion state
enum MotionState {
  STOPPED,
  ACCELERATING,
  CONSTANT_SPEED,
  DECELERATING,
  EMERGENCY_STOP
};

// Global variables
bool running = false;
MotionState currentState = STOPPED;
uint32_t currentSpeedHz = MIN_SPEED_HZ;
uint32_t targetSpeedHz = MAX_SPEED_HZ;
bool currentDirection = true; // true = forward/right, false = backward/left
unsigned long lastRampUpdate = 0;
bool endstopTriggered = false;

// Function declarations
void setDirection(bool forward);
void setSpeedHz(uint32_t hz);
void start();
void stop();
void emergencyStop();
bool checkEndstops();
void updateRamping();
void moveToSpeed(uint32_t targetHz);

void setDirection(bool forward) {
  currentDirection = forward;
  digitalWrite(DIR_PIN, forward ? HIGH : LOW);
}

void setSpeedHz(uint32_t hz) {
  if (hz < MIN_SPEED_HZ) hz = MIN_SPEED_HZ;
  if (hz > MAX_SPEED_HZ) hz = MAX_SPEED_HZ;

  currentSpeedHz = hz;
  if (running) {
    ledcWriteTone(LEDC_CHANNEL, hz);
  }
}

void start() {
  if (running) return;

  // Check endstops before starting
  if (checkEndstops()) {
    Serial.println("Cannot start: Endstop active!");
    return;
  }

  ledcAttachPin(STEP_PIN, LEDC_CHANNEL);
  ledcSetup(LEDC_CHANNEL, currentSpeedHz, LEDC_RES);
  ledcWrite(LEDC_CHANNEL, (1 << (LEDC_RES - 1))); // 50% duty
  running = true;
  currentState = ACCELERATING;
  lastRampUpdate = millis();

  Serial.println("Motor started");
}

void stop() {
  if (!running) return;

  currentState = DECELERATING;
  targetSpeedHz = MIN_SPEED_HZ;

  Serial.println("Stopping motor...");
}

void emergencyStop() {
  if (!running) return;

  ledcDetachPin(STEP_PIN);
  digitalWrite(STEP_PIN, LOW);
  running = false;
  currentState = STOPPED;
  currentSpeedHz = MIN_SPEED_HZ;
  endstopTriggered = false;

  Serial.println("EMERGENCY STOP!");
}

bool checkEndstops() {
  bool leftActive = digitalRead(ENDSTOP_LEFT_PIN) == LOW;
  bool rightActive = digitalRead(ENDSTOP_RIGHT_PIN) == LOW;

  // Check if we're moving toward an active endstop
  if ((currentDirection && rightActive) || (!currentDirection && leftActive)) {
    if (!endstopTriggered) {
      endstopTriggered = true;
      Serial.println(currentDirection ? "Right endstop hit!" : "Left endstop hit!");
      return true;
    }
  } else {
    endstopTriggered = false;
  }

  return false;
}

void updateRamping() {
  if (!running || currentState == STOPPED) return;

  unsigned long now = millis();
  if (now - lastRampUpdate < RAMP_UPDATE_INTERVAL_MS) return;

  lastRampUpdate = now;

  // Check endstops during motion
  if (checkEndstops()) {
    emergencyStop();
    return;
  }

  uint32_t speedChange = (ACCEL_STEPS_PER_SEC2 * RAMP_UPDATE_INTERVAL_MS) / 1000;

  switch (currentState) {
    case ACCELERATING:
      if (currentSpeedHz < targetSpeedHz) {
        currentSpeedHz += speedChange;
        if (currentSpeedHz >= targetSpeedHz) {
          currentSpeedHz = targetSpeedHz;
          currentState = CONSTANT_SPEED;
          Serial.println("Reached target speed");
        }
        setSpeedHz(currentSpeedHz);
      } else {
        currentState = CONSTANT_SPEED;
      }
      break;

    case DECELERATING:
      if (currentSpeedHz > targetSpeedHz) {
        if (currentSpeedHz > speedChange) {
          currentSpeedHz -= speedChange;
        } else {
          currentSpeedHz = MIN_SPEED_HZ;
        }

        if (currentSpeedHz <= targetSpeedHz) {
          // Fully stopped
          ledcDetachPin(STEP_PIN);
          digitalWrite(STEP_PIN, LOW);
          running = false;
          currentState = STOPPED;
          currentSpeedHz = MIN_SPEED_HZ;
          Serial.println("Motor stopped");
        } else {
          setSpeedHz(currentSpeedHz);
        }
      }
      break;

    case CONSTANT_SPEED:
      // Just maintain current speed and check endstops
      break;

    default:
      break;
  }
}

void moveToSpeed(uint32_t targetHz) {
  if (targetHz < MIN_SPEED_HZ) targetHz = MIN_SPEED_HZ;
  if (targetHz > MAX_SPEED_HZ) targetHz = MAX_SPEED_HZ;

  targetSpeedHz = targetHz;

  if (!running) {
    start();
  } else if (currentSpeedHz < targetHz) {
    currentState = ACCELERATING;
  } else if (currentSpeedHz > targetHz) {
    currentState = DECELERATING;
  }
}

void handleMaskCommand(const String &command) {

}


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String command = String(value.c_str());
      Serial.println(command);
      // if (command == "GET_CONFIG") {
      //   String config = getConfigJson();
      //   pCharacteristic->setValue(config.c_str());
      //   pCharacteristic->notify();
      // } else if (command.startsWith("MASK:")) {
      //   handleMaskCommand(command);
      // }
      // FastLED.show();
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-C3 Stepper Controller with Ramping and Endstops");

  // Configure pins
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  // Configure endstop pins with pullups (endstops connect to GND when triggered)
  pinMode(ENDSTOP_LEFT_PIN, INPUT_PULLUP);
  pinMode(ENDSTOP_RIGHT_PIN, INPUT_PULLUP);

  // Enable the driver (LOW = enabled on A4988)
  digitalWrite(EN_PIN, LOW);

  // Initial setup
  setDirection(true);
  digitalWrite(STEP_PIN, LOW);

  Serial.println("Motor ready.");
  Serial.println("Commands: 's' = start, 'x' = stop, 'e' = emergency stop, 'd' = change direction");

  // Initialize BLE
  BLEDevice::init("Ledzz mask");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902()); // Required for notifications
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE LED Control Ready!");
}

void loop() {
  // Update ramping continuously
  updateRamping();

  // Handle serial commands for testing
  if (Serial.available()) {
    char cmd = Serial.read();
    switch (cmd) {
      case 's':
        Serial.println("Starting motor...");
        moveToSpeed(MAX_SPEED_HZ);
        break;

      case 'x':
        Serial.println("Stopping motor...");
        stop();
        break;

      case 'e':
        Serial.println("Emergency stop!");
        emergencyStop();
        break;

      case 'd':
        if (running) {
          Serial.println("Cannot change direction while running. Stop first.");
        } else {
          setDirection(!currentDirection);
          Serial.println(currentDirection ? "Direction: Forward/Right" : "Direction: Backward/Left");
        }
        break;

      case '1':
        moveToSpeed(500);
        break;

      case '2':
        moveToSpeed(1000);
        break;

      case '3':
        moveToSpeed(1500);
        break;

      case '4':
        moveToSpeed(MAX_SPEED_HZ);
        break;
    }
  }

  // Example motion pattern (comment out if you want manual control only)
  static unsigned long lastDirectionChange = 0;
  static bool autoMode = false; // Set to true to enable automatic back-and-forth

  if (autoMode && millis() - lastDirectionChange > 5000) {
    if (currentState == CONSTANT_SPEED || currentState == STOPPED) {
      stop();
      delay(1000); // Wait for stop
      if (currentState == STOPPED) {
        setDirection(!currentDirection);
        moveToSpeed(MAX_SPEED_HZ);
        lastDirectionChange = millis();
      }
    }
  }

  delay(10); // Small delay to prevent overwhelming the system
}