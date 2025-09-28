#include <Arduino.h>
#include <stepper.h>
#include <ble.h>
#include <Preferences.h>


Stepper stepper;
Ble ble;
Preferences preferences;


void saveSpeed(int speed) {
    preferences.begin("stepper", false);
    preferences.putInt("speed", speed);
    preferences.end();
}

int loadSpeed() {
    preferences.begin("stepper", true);
    int speed = preferences.getInt("speed", 0);
    preferences.end();
    return speed;
}

void setup() {
    Serial.begin(115200);
    stepper.begin();
    stepper.setSpeedAndStartLoop(loadSpeed());
    ble.addCommand("speed", [](const JsonDocument &doc) {
        int speed = doc["speed"].as<int>();
        saveSpeed(speed);
        stepper.setSpeedAndStartLoop(speed);
    });

    ble.begin();
}

void loop() {
    while (1) {
        ble.poll();
    }
}
