#include <Arduino.h>
#include <esp_sleep.h>
#include "config.h"

void goToSleep() {
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
    esp_deep_sleep_start(); // after timer expires, ESP32 boots from setup()
}