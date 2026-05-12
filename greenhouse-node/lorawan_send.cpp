#include <Arduino.h>
#include <HardwareSerial.h>

// Creds:
// DevEUI: 0004A30B00F19BF4 (Remember that this value is unique to your RN2483 module)
// JoinEUI: BE7A000000001465
// AppKey: 19B357CC934F3AC997EE4A6CDA83FB03

static const char* DEVEUI = "0004A30B00F19BF4";
static const char* JoinEUI = "BE7A000000001465";
static const char* APPKey = "19B357CC934F3AC997EE4A6CDA83FB03";
// We'll use UART1 for communication with our module.
static HardwareSerial loraSerial(1);
// Pins for UART1:
static const int RXD2 = 18;
static const int TXD2 = 19;
static const int RST = 23;

// Send command to RN2483 and return the response.
static String sendCmd(const char* cmd) {
    Serial.print(">> ");
    Serial.println(cmd);
    loraSerial.println(cmd);
    String resp = loraSerial.readStringUntil('\n');
    resp.trim(); // Remove trailing characters like \r\n
    Serial.print("<< ");
    Serial.println(resp);
    return resp;
}

// lorawan setup function. This will reset the module, set credentials and attempt to join Cibicom
// It will be a boolean so only returns success/fail.
bool lorawan_setup() {
    loraSerial.begin(57600, SERIAL_8N1, RXD2, TXD2);
    loraSerial.setTimeout(15000); // Join process can take some time, so we set a long timeout (15 seconds)
    pinMode(RST, OUTPUT);
    digitalWrite(RST, HIGH);
    digitalWrite(RST, LOW);
    delay(200);
    digitalWrite(RST, HIGH);
    delay(500);
    loraSerial.readStringUntil('\n'); // Read startup message so we're ready to send commands (clearing the startup message from buffer basically))
    
    sendCmd("mac reset 868"); // Reset to EU868 region
    
    String cmd;
    cmd = "mac set deveui " + String(DEVEUI);
    sendCmd(cmd.c_str());

    cmd = "mac set appeui " + String(JoinEUI);
    sendCmd(cmd.c_str());

    cmd = "mac set appkey " + String(APPKey);
    sendCmd(cmd.c_str());

    sendCmd("mac set adr on"); // Enable adaptive data rate (ADR). Allows Cibicom to adjust SF dynamically based on signal quality, which can improve battery life and stability of our connection.
    sendCmd("mac save"); // Save the settings to non-volatile memory (so they persist after reset)

    Serial.println("Attempting join...");
    loraSerial.println("mac join otaa"); // Start the join process using OTAA

    String resp1 = loraSerial.readStringUntil('\n');
    resp1.trim();
    Serial.print("Response 1: ");
    Serial.println(resp1);
    if (resp1 != "ok") {
        Serial.println("JOIN FAILED — invalid command or parameters");
        return false;
    }

    loraSerial.setTimeout(30000); // Join process can take a long time, so we set an even longer timeout (30 seconds) for the second response
    String resp2 = loraSerial.readStringUntil('\n');
    resp2.trim();
    Serial.print("Response 2: ");
    Serial.println(resp2);

    loraSerial.setTimeout(15000); // Reset timeout to default for future commands
    if (resp2.indexOf("accepted") >= 0) {
        Serial.println("JOIN SUCCESS");
        return true;
    } else {
        Serial.println("JOIN FAILED — no coverage or wrong credentials");
        return false;
    }
}
// Send function that will send some payload to Cibicoms network server.
// data = byte array (MUST BE BYTE ARRAY!!) to send
// data_len = number of bytes
// port = LoRaWAN port (1-255. We will use 1 as default). Remember from our exercise that we could choose this rather loosely
bool lorawan_send(const unsigned char* data, size_t data_len, int port){
    // Convert our byte array to a hex string, since the RN2483 expects its commands to be with hex.
    String hex = "";
    for (size_t i=0; i<data_len; i++) {
        if(data[i]<0x10) hex+="0"; // Add leading zero for single digit hex values
        hex+=String(data[i],HEX);
    }
    
    String cmd = "mac tx uncnf " + String(port) + " " + hex; // uncnf = unconfirmed message (no ACK required). We can also use "cnf" for confirmed messages, but that requires more complex handling of the response.
    Serial.print(">> ");
    Serial.println(cmd);
    loraSerial.println(cmd);

    String resp1 = loraSerial.readStringUntil('\n');
    resp1.trim();
    Serial.print("<< ");
    Serial.println(resp1);
    if (resp1 != "ok") {
        Serial.print("TX command rejected : ");
        Serial.println(resp1);
        return false;
    }

    loraSerial.setTimeout(30000); // Transmission can also take a long time, so we set a longer timeout for the response
    String resp2 = loraSerial.readStringUntil('\n');
    resp2.trim();
    Serial.print("<< ");
    Serial.println(resp2);
    loraSerial.setTimeout(15000); // Reset timeout to default for future commands
    if (resp2.indexOf("mac_tx_ok") >= 0) {
        Serial.println("TX SUCCESS");
        return true;
    } else {
        Serial.print("TX failed: ");
        Serial.println(resp2);
        return false;
    }

}
