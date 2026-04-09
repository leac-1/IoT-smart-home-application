#include <Arduino.h>
#include "pins.h"

void initLed() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void setLed(bool on) {
    digitalWrite(LED_PIN, on ? HIGH : LOW);
}

bool getLedState() {
    return digitalRead(LED_PIN) == HIGH;
}