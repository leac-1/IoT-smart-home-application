#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient webSocket;
// Cibicom WebSocket server (HTTPS) and token
static const char* WS_HOST = "iotnet.teracom.dk";
static const int   WS_PORT = 443;
static const char* WS_PATH = "/app?token=vnoUZQAAABFpb3RuZXQudGVyYWNvbS5ka7UQ2G66F7G46LgTOrqT8n4=";

extern void blynk_send(int pin, double value);
extern void blynk_send(int pin, const char* value);

// The Websocket forwards the payload data as a hexstring, so we must reformat it to bytes
// The bytes can then later be reformated for and sent using Blynk.
static size_t hexToBytes(const char* hex, unsigned char* out, size_t max_len) {
    size_t hex_len = strlen(hex);
    size_t byte_count = hex_len / 2;
    if (byte_count > max_len) byte_count = max_len; // Memory safety

    for (size_t i = 0; i < byte_count; i++) {
        char byte_str[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
        out[i] = (unsigned char)strtol(byte_str, NULL, 16); // We use "null", since we don't need to know where parssing stopped, since we know that it contains exactly 2 valid bytes.
    }
    return byte_count; // Returns number of bytes that out now contains.
}
// The library has different types of "events". We only want to act on reception of a message
// We also just print connection status in the serial monitor to keep track
static void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {

        case WStype_CONNECTED:
            Serial.println("WebSocket connected to Cibicom");
            break;

        case WStype_DISCONNECTED:
            Serial.println("WebSocket disconnected — will auto-reconnect");
            break;

        // When we receive a message, it is in JSON format, so we must deserialize it
        case WStype_TEXT: {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload, length);
            if (error) return;

            const char* cmd = doc["cmd"]; // The format specifies what "type" of message we have. "rx" means, that actual data has arrived.
            if (cmd == NULL || strcmp(cmd, "rx") != 0) return;

            const char* eui  = doc["EUI"];
            const char* data = doc["data"];
            int port         = doc["port"];
            if (eui == NULL || data == NULL) return;

            // Print the data that arrived, and from what device it came.
            Serial.print("LoRaWAN RX from ");
            Serial.print(eui);
            Serial.print(": ");
            Serial.println(data);

            // Write the data into an array
            unsigned char bytes[32];
            size_t byte_count = hexToBytes(data, bytes, sizeof(bytes));

            // We just specify that the port will be 1 in Cibicom. This doesn't really matter in this case, since we only send 1 type of data (temperature).
            if (port == 1 && byte_count >= 2) {
                int16_t temp_raw = (bytes[1] << 8) | bytes[0];
                float celsius = temp_raw / 10.0;
                blynk_send(0, celsius);
                Serial.print("Temperature via LoRaWAN: ");
                Serial.println(celsius);
            }

            break;
        }

        default:
            break;
    }
}

// Setup the websocket connection and "bind" our event function.
void ws_setup() {
    webSocket.beginSSL(WS_HOST, WS_PORT, WS_PATH);
    webSocket.onEvent(onWebSocketEvent);
    webSocket.setReconnectInterval(5000);
    Serial.println("WebSocket connecting to Cibicom...");
}

void ws_loop() {
    webSocket.loop();
}