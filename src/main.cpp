#include <Arduino.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Pin definitions
constexpr int STEP_PIN = 7;
constexpr int DIR_PIN = 6;
constexpr int EN_PIN = 4;
constexpr int ENDSTOP_LEFT_PIN = 2;
constexpr int ENDSTOP_RIGHT_PIN = 1;

constexpr int ACCEL = 100;
constexpr int MAX_SPEED = 500;
constexpr int PULSE_WIDTH_US = 50;
constexpr int MAIN_LOOP_DELAY_MS = 1;

// Motion state
enum MotionState {
    STOPPED,
    ACCELERATING,
    CONSTANT_SPEED,
    DECELERATING,
    EMERGENCY_STOP
};

uint speed = 0;
int position = 0;
int positionMin = 0;
int positionMax = 0;
int direction = 1;

unsigned long lastStep = 0;


void setDirection(bool forward) {
    direction = forward;
    digitalWrite(DIR_PIN, forward ? HIGH : LOW);
}

void rampToSpeedTask(void *parameter) {
    int target = *(int *) parameter; // Dereference the pointer to get the actual value
    Serial.printf("rampToSpeed %i -> %i \n", speed, target); // Fixed: was printing undefined variable

    uint unsignedTarget = abs(target);
    direction = target > 0 ? 1 : -1;


    while (abs(static_cast<int>(speed - target)) >= ACCEL) {
        Serial.printf("abs(static_cast<int>(speed - target)) = %i \n", abs(static_cast<int>(speed - target)));

        if (speed < target) {
            speed += ACCEL;
        } else if (speed > target) {
            speed -= ACCEL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        speed = constrain(speed, 0, MAX_SPEED);
        Serial.printf("speed = %i \n", speed);
    }

    speed = abs(target); // Fixed: use the actual target value, not unsignedTarget

    // Delete this task when done
    vTaskDelete(NULL);
}

TaskHandle_t rampToSpeed(int target) {
    // Allocate memory for the target value to pass to the task
    int *targetPtr = new int(target);
    TaskHandle_t xHandle = NULL;

    xTaskCreate(rampToSpeedTask, "rampToSpeed", 1024, targetPtr, 1, &xHandle);

    return xHandle;
}

void motor_loop(TimerHandle_t xTimer) {
    if (speed == 0) {
        return;
    }

    unsigned long now = micros();

    if ((now - lastStep) > 1000000 / speed) {
        lastStep = now;
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(PULSE_WIDTH_US);
        digitalWrite(STEP_PIN, LOW);
        position += direction;
    }

    Serial.printf("position %i, speed %i", position, speed);
    Serial.println();
}


void calibrate() {
    Serial.println("Starting calibration...");

    TaskHandle_t xHandle = rampToSpeed(MAX_SPEED);
    while (digitalRead(ENDSTOP_LEFT_PIN) == HIGH && digitalRead(ENDSTOP_RIGHT_PIN) == HIGH) {
    }

    if (xHandle != NULL) {
        vTaskDelete(xHandle);
    }

    speed = 0;

    positionMin = position;

    TaskHandle_t xHandle1 = rampToSpeed(-MAX_SPEED);
    vTaskDelay(200);
    while (digitalRead(ENDSTOP_LEFT_PIN) == HIGH && digitalRead(ENDSTOP_RIGHT_PIN) == HIGH) {
    }
    if (xHandle1 != NULL) {
        vTaskDelete(xHandle1);
    }
    positionMax = position;
    speed = 0;
    direction = 1;

    Serial.printf("Calibration done, range (%i, %i)", positionMin, positionMax);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    // while (!Serial) {
    // ; // wait for serial port to connect. Needed for native USB
    // }
     delay(500);
    Serial.println("Setup start.");
    pinMode(DIR_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);

    pinMode(ENDSTOP_LEFT_PIN, INPUT_PULLUP);
    pinMode(ENDSTOP_RIGHT_PIN, INPUT_PULLUP);

    digitalWrite(EN_PIN, LOW);
    digitalWrite(STEP_PIN, LOW);

    Serial.println("Setup done.");
    delay(1000);

    // xTaskCreate(motor_loop, "motor_loop", 1024, NULL, 1, NULL);

    TimerHandle_t timer = xTimerCreate
    (
        "motor_loop",
        pdMS_TO_TICKS(10),
        pdTRUE,
        (void *) 0,
        motor_loop
    );

    if (NULL == timer) {
        assert(pdFAIL);
        return;
    }
    BaseType_t timerResult = xTimerStart(timer, 0xffff);
    if (pdTRUE != timerResult) {
        assert(pdFAIL);
    }
    // vTaskStartScheduler();

    calibrate();
}

void loop() {
    while (1) {

    }
    // motor_loop();
    // delay(MAIN_LOOP_DELAY_MS);
}
