// Blynk creds. We have to define before inclusion of library, since the library uses these values.
#define BLYNK_TEMPLATE_ID "TMPL5c36ZH4-A"
#define BLYNK_TEMPLATE_NAME "Smart Home"
#define BLYNK_AUTH_TOKEN "MKiupt-A7NAJE8BBpkNkbqPi5np3CFvM"

#define BLYNK_PRINT Serial // Print debug messages to serial monitor

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Blynk setup function.
// ssid = WiFi network name
// pw = WiFi password

void blynk_setup(const char* ssid, const char* pw) {
    //Since the .begin() function from Blynk is blocking and will never return if it doesnt connect to the WiFi or Blynk cloud, we have to do the connections independently of each other:
    WiFi.begin(ssid, pw);
    Serial.print("Connecting to WiFi");
    // Manual timeout for WiFi connection
    for (int i = 0; i<20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected!" : "Failed to connect to WiFi");
    if (WiFi.status() != WL_CONNECTED) return; // If we failed to connect to WiFi, we cannot connect to Blynk, so we return early.
    // Blynk connection AFTER WiFi is connected, since Blynk needs WiFi.
    Blynk.config(BLYNK_AUTH_TOKEN);
    // The .connect() function from the Blynk library takes an integer as a timeout arguement in seconds. Handy!
    Serial.println(Blynk.connect(10) ? "Blynk connected" : "Blynk failed to connect");
}

// We can do function overloading to handle both numeric and string values for sending stuff to the Blynk cloud 
// This might be a bit overkill to do wrappers, but it's a nice to have to just use blynk_send(pin,value)
// Blynk send() function. This will send a numeric value to a specific virtual pin in Blynk.
// pin = virtual pin NUMBER (0 = V0, 1 = V1, etc.)
// value = number to send
// Example: blynk_send(0,23.5): Updates V0 (temperature) to 23.5 degrees
void blynk_send(int pin, double value) {
    Blynk.virtualWrite(pin, value);
}
// Blynk send() function. This will send a string value to a specific virtual pin in Blynk.
// Same as before. Now value is a pointer containing a string.
// Example: blynk_send(2,"open"); Updates V2 (curtain state) to "open"
void blynk_send(int pin, const char* value) {
    Blynk.virtualWrite(pin, value);
}

// We use a wrapper for Blynk.run(), because we get problems if our "main" code also wants to create blynk objects, since the library itself creates objects when included
void blynk_loop(){
    Blynk.run();
}
/*
// EXAMPLE USE!!!
This will send some data to our 4 virtual pins in blynk every 5 seconds. It works fine with my home WiFi and should for any WiFi connection.

#include <Arduino.h>

extern void blynk_setup(const char* ssid, const char* pw);
extern void blynk_send(int pin, double value);
extern void blynk_send(int pin, const char* value);
extern void blynk_loop();

void setup() {
    Serial.begin(115200);
    delay(1000);
    blynk_setup("YOUR SSID", "YOUR PASSWORD");
}

void loop() {
    blynk_loop();

    blynk_send(0, 23.5);
    blynk_send(1, 450);
    blynk_send(2, "open");
    blynk_send(3, "off");

    Serial.println("Data sent to Blynk");
    delay(5000);
}
*/