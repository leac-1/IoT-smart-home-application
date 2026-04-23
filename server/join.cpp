#include <Arduino.h>
#include "LoraCom.h"

bool checkForJoinRequest() {
    // Check for join request
    if (loraSerial.available()) {
        String request = loraSerial.readStringUntil('\n');
        Serial.print("Received: ");
        Serial.println(request);
        if (request.startsWith("radio rx")) {
            int startIdx = request.indexOf("radio_rx") + 9;
            String hexPayload = request.substring(startIdx);
            if (hexPayload.length() >= 2 && hexPayload[0] == 0x0F) {
                return true;
            }
        }
    }
    return false;
}