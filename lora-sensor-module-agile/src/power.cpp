#include <Arduino.h>
#include <esp_sleep.h>
#include "config.h"

void goToSleep(unsigned long seconds) {
    Serial.printf("Sleeping for %lu seconds\n", seconds);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
}