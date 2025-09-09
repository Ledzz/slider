//
// Created by ledzz on 9 Sep 2025.
//

enum Status {
    Idle,
    Calibrating,
    Ready,
    Running
};

// Pin definitions
constexpr int STEP_PIN = 7;
constexpr int DIR_PIN = 6;
constexpr int EN_PIN = 4;
constexpr int ENDSTOP_LEFT_PIN = 2;
constexpr int ENDSTOP_RIGHT_PIN = 1;
constexpr int PULSE_WIDTH_US = 50;
constexpr int CALIBRATION_SPEED = 10000;
constexpr int LINEAR_SPEED = 8000;


class Stepper {
private:
    int speed = 0;
    Status status = Idle;
    bool endstopReached = false;
    SemaphoreHandle_t endstopSemaphore;
    unsigned long lastStep = micros();
    int position = 0;

    int positionMin = 0;
    int positionMax = 0;

    void step(void) {
    };

    void handleEndstop() {
        Serial.println("handleEndstop");
        xSemaphoreGive(endstopSemaphore);
        portYIELD_FROM_ISR();
    };

    void endStopLoop() {
        bool newReached = (digitalRead(ENDSTOP_LEFT_PIN) == LOW || digitalRead(ENDSTOP_RIGHT_PIN) == LOW);
        if (newReached != endstopReached) {
            endstopReached = newReached;

            if (endstopReached) {
                handleEndstop();
            }
        }
    };

    void motorLoop() {
        if (speed == 0) {
            return;
        }

        unsigned long now = micros();

        if ((now - lastStep) > 1000000 / abs(speed)) {
            lastStep = now;
            digitalWrite(STEP_PIN, HIGH);
            delayMicroseconds(PULSE_WIDTH_US);
            digitalWrite(STEP_PIN, LOW);
            position += get_direction();
        }
    }

    int get_direction() {
        if (speed == 0) {
            return 0;
        }

        return speed > 0 ? 1 : -1;
    }

    static void endStopLoopCallback(TimerHandle_t xTimer) {
        Stepper *stepper = static_cast<Stepper *>(pvTimerGetTimerID(xTimer));
        stepper->endStopLoop();
    }

    static void motorLoopCallback(TimerHandle_t xTimer) {
        Stepper *stepper = static_cast<Stepper *>(pvTimerGetTimerID(xTimer));
        stepper->motorLoop();
    }

public:
    void begin() {
        Serial.println("Setup start.");
        pinMode(DIR_PIN, OUTPUT);
        pinMode(STEP_PIN, OUTPUT);
        pinMode(EN_PIN, OUTPUT);

        pinMode(ENDSTOP_LEFT_PIN, INPUT_PULLUP);
        pinMode(ENDSTOP_RIGHT_PIN, INPUT_PULLUP);

        digitalWrite(EN_PIN, LOW);
        digitalWrite(STEP_PIN, LOW);

        endstopSemaphore = xSemaphoreCreateBinary();

        TimerHandle_t endstopTimer = xTimerCreate("endstopLoop", pdMS_TO_TICKS(10),
                                                  pdTRUE,
                                                  (void *) this, endStopLoopCallback);

        if (NULL == endstopTimer) {
            assert(pdFAIL);
            return;
        }
        if (pdTRUE != xTimerStart(endstopTimer, 0xffff)) {
            assert(pdFAIL);
        }


        TimerHandle_t motorTimer = xTimerCreate("motorLoop", pdMS_TO_TICKS(1),
                                                  pdTRUE,
                                                  (void *) this, motorLoopCallback);

        if (NULL == motorTimer) {
            assert(pdFAIL);
            return;
        }
        if (pdTRUE != xTimerStart(motorTimer, 0xffff)) {
            assert(pdFAIL);
        }

        Serial.println("Setup done.");
        calibrate();
    };

    void setSpeedInstant(int target) {
        speed = target;
        digitalWrite(DIR_PIN, target > 0? HIGH : LOW);
    };

    void setPosition(int target) {
        Serial.printf("setPosition %i", target);
        int direction = target > position ? 1 : -1;

        setSpeedInstant(direction * LINEAR_SPEED);

        while ((direction > 0 && target > position) || (direction < 0 && target < position)) {
            Serial.println(position);
        }

        setSpeedInstant(0);
    }

    void calibrate() {
        Serial.println("calibrate");

        status = Calibrating;

        setSpeedInstant(CALIBRATION_SPEED);

        xSemaphoreTake(endstopSemaphore, portMAX_DELAY);
        positionMax = position;

        setSpeedInstant(-CALIBRATION_SPEED);

        xSemaphoreTake(endstopSemaphore, portMAX_DELAY);
        positionMin = position;

        setSpeedInstant(0);

        status = Ready;

        Serial.printf("Calibration done, range: [%i, %i]", positionMin, positionMax);

        setPosition(floor((positionMax + positionMin) / 2));
    };
};
