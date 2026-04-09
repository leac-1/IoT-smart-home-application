#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"

#define RXD2 18
#define TXD2 19
#define RST 23

unsigned char* source_address = NULL;

void setup() {
    source_address = (unsigned char*)malloc(1);
    if (source_address != NULL) {
        *source_address = 0x01;
    }
}

void loop() {
    
}

extern "C" void app_main(void) {
    initArduino();

    printf("Hello world!\n");
}
