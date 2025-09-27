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

static inline float now_s() {
    // examples:
    // return micros() * 1e-6f;                // Arduino
    return xTaskGetTickCount() / 1000.0f;   // FreeRTOS ms tick
    // return hal_timer_read() * TICK_TO_S;    // your HAL
}

struct ProfileGen {
    float p0, p1, T;
    float t0;                // start time (s)
    float v_limit, a_limit;  // optional safety caps
    float (*s)(float);
    float (*ds)(float);

    void init(float start, float target, float duration_s,
              float (*s_fn)(float), float (*ds_fn)(float),
              float v_max=INFINITY, float a_max=INFINITY) {
        p0 = start; p1 = target; T = duration_s;
        s = s_fn; ds = ds_fn;
        v_limit = v_max; a_limit = a_max;
        t0 = now_s();
    }

    // Compute setpoints for the *current* time.
    void sample(float &pos_sp, float &vel_sp, bool &done) {
        float t = now_s() - t0;
        if (t >= T) {
            pos_sp = p1; vel_sp = 0.0f; done = true; return;
        }
        if (t <= 0) t = 0;

        float tau   = t / T;                 // normalized time
        float s_val = s(tau);
        float dsdt  = ds(tau) / T;           // (ds/dτ)*(dτ/dt) = ds/dτ / T

        // Raw setpoints
        float dp = (p1 - p0);
        pos_sp = p0 + dp * s_val;
        vel_sp = dp * dsdt;

        // Optional: clamp velocity (and acceleration via caller if needed)
        if (fabsf(vel_sp) > v_limit) vel_sp = copysignf(v_limit, vel_sp);

        done = false;
    }
};


class Stepper {
private:
    int speed = 0;
    Status status = Idle;
    bool endstopReached = false;
    SemaphoreHandle_t endstopSemaphore;
    SemaphoreHandle_t targetSemaphore;
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
        targetSemaphore = xSemaphoreCreateBinary();

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

    void setPositionMaxSpeed(int target) {
        Serial.printf("setPosition %i", target);
        int direction = target > position ? 1 : -1;

        setSpeedInstant(direction * LINEAR_SPEED);

        while ((direction > 0 && target > position) || (direction < 0 && target < position)) {
        }

        setSpeedInstant(0);
    }

    // Example S-curve (quintic minimum-jerk)
    static float s_quintic(float x){ float x2=x*x,x3=x2*x,x4=x3*x,x5=x4*x; return 10*x3-15*x4+6*x5; }
    static float ds_quintic(float x){ float x2=x*x,x3=x2*x,x4=x3*x; return 30*x2-60*x3+30*x4; }


    void setPositionRamp(int target, int duration) {
        // Usage:
        ProfileGen prof;
        prof.init(position, target, 2.0f, s_quintic, ds_quintic, /*v_max=*/2000.0f);
        // In your main/control loop (any timing):

        float p_sp, v_sp; bool done;


        while(!done) {
            prof.sample(p_sp, v_sp, done);

            setSpeedInstant(v_sp);
        }


        xSemaphoreGive(targetSemaphore);
        portYIELD_FROM_ISR();

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

        // setPositionMaxSpeed(floor((positionMax + positionMin) / 2));

        setPositionRamp(floor((positionMax + positionMin) / 2), 2);

        xSemaphoreTake(targetSemaphore, portMAX_DELAY);
        setPositionRamp(positionMax, 2 );

        xSemaphoreTake(targetSemaphore, portMAX_DELAY);
        setPositionRamp(positionMin, 2);
    };
};
