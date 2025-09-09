#include <Arduino.h>
#include <stepper.h>


Stepper stepper;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("1");
    delay(1000);
    Serial.println("2");

    delay(1000);

    stepper.begin();

}

void loop() {
    while (1) {
    }
}
