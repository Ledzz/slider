#include <Arduino.h>
#include <stepper.h>
#include <ble.h>

Stepper stepper;
Ble ble;


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("1");
    delay(1000);
    Serial.println("2");

    delay(1000);


    stepper.begin();

    ble.addCommand("speed", [](const JsonDocument &doc) {
        Serial.println("command");

        int speed = doc["speed"].as<int>();

        stepper.setSpeedAndStartLoop(speed);
    });

    // ble.addCommand("speed", [](const JsonDocument &doc) {
    //     int speed = doc["speed"].as<int>();
    //
    //     stepper.setSpeedAndStartLoop(speed);
    // });
    ble.begin();
}

void loop() {
    while (1) {
        ble.poll();
    }
}
