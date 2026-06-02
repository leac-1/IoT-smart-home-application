# Curtain Node for IoT Smart Home

A temperature-controlled curtain actuator node using LoRa communication for the DTU IoT Smart Home system.

## Hardware

### Components
- **Microcontroller**: ESP32 (e.g., ESP32-DOIT-DEVKIT-V1)
- **LoRa Module**: RN2483 or RN2384 over UART (57600 baud)
- **Sensor**: DHT11 (temperature & humidity)
- **Actuator**: TowerPro SG90 servo motor (9-gram micro servo)

### Pinout

| Component | ESP32 GPIO | Notes |
|-----------|-----------|-------|
| DHT11 Data | GPIO 4 | Uses existing sensor network pin |
| SG90 Signal | GPIO 13 | PWM output for servo control |
| RN2483 RX | GPIO 19 | UART RX for LoRa module |
| RN2483 TX | GPIO 18 | UART TX for LoRa module |
| RN2483 RST | GPIO 23 | Reset pin for LoRa module |

### Power Supply

**Important**: The SG90 servo must be powered from an external 5V supply, NOT from the ESP32's 3.3V pin.

- **SG90 Brown wire**: Connect to GND (common with external supply GND)
- **SG90 Red wire**: Connect to external 5V regulated supply
- **SG90 Orange/Yellow wire**: Connect to GPIO 13

**Ensure GND is common between ESP32, LoRa module, and external servo power supply.**

### Physical Mounting

The servo rotates approximately 0° to 180°. Current configuration:
- **0° (SERVO_OPEN_ANGLE)**: Curtains fully open (counter-clockwise)
- **90° (SERVO_CLOSED_ANGLE)**: Curtains fully closed (clockwise)

Adjust `SERVO_OPEN_ANGLE` and `SERVO_CLOSED_ANGLE` constants in `include/pins.h` if your mechanical setup is different.

## Behavior

### Temperature-Based Control

The curtain node monitors room temperature and adjusts curtains automatically:

- **Temperature ≥ 26.0°C**: Close curtains (clockwise) to block sunlight
- **Temperature ≤ 20.0°C**: Open curtains (counter-clockwise) to let sunlight in
- **20.0°C < Temperature < 26.0°C**: Maintain current state (hysteresis)

### Network Communication

- **Node ID**: Hardcoded to `0x06` (configure in `src/main.cpp` if needed)
- **Destination**: Server/Gateway at address `0x00`
- **Protocol**: AES-128 CCM encrypted LoRa packets

### Packet Types

#### State Report (0x03)
Sent after servo movement or periodically:

```
Byte 0: Device type = 0x04 (curtain node)
Byte 1: State (0=unknown, 1=open, 2=closed)
Byte 2-3: Temperature × 10 (signed int16, big-endian)
Byte 4-5: Humidity × 10 (unsigned int16, big-endian)
```

#### Command Reception (0x02) ⚠️ Optional
If server sends commands:

```
Byte 0: Command code
  0x01 = Open
  0x02 = Close
  0x03 = Toggle
  0x04 = Request status
```

## Building & Uploading

### Prerequisites
- PlatformIO installed
- USB cable connected to ESP32

### Build
```bash
cd curtain-node-agile/
platformio run -e esp32doit-devkit-v1
```

### Upload
```bash
platformio run -e esp32doit-devkit-v1 --target upload
```

### Monitor Serial Output
```bash
platformio device monitor -e esp32doit-devkit-v1 --baud 115200
```

## Testing Procedure

1. **Hardware Assembly**
   - Connect DHT11 to GPIO 4 (with pull-up if needed)
   - Connect SG90 signal wire to GPIO 13
   - Connect SG90 power (5V) and GND separately from external supply
   - Ensure common GND between all components

2. **Firmware Upload**
   ```bash
   platformio run -e esp32doit-devkit-v1 --target upload
   ```

3. **Boot & Initialization**
   - Open serial monitor at 115200 baud
   - Observe: "Curtain Node Starting" banner
   - Confirm LoRa initialization: "LoRa ready"
   - Servo should move to initial position (OPEN)

4. **Temperature Response**
   - Heat DHT11 probe above 26°C (e.g., with warm water)
   - Observe: Serial logs show decision to close, servo moves to 90°
   - Observe: Encrypted state packet is sent
   - Cool DHT11 probe below 20°C (e.g., with ice)
   - Observe: Servo moves to 0°, state packet sent

5. **Server Reception**
   - On gateway/server, monitor incoming packets
   - Confirm packet type `0x03` with device type `0x04`
   - Confirm MIC verification passes
   - Confirm Blynk virtual pin updates (if configured)

6. **Calibration**
   - If servo direction is reversed, swap `SERVO_OPEN_ANGLE` and `SERVO_CLOSED_ANGLE`
   - If servo angles need adjustment for your mechanical setup:
     - Edit `include/pins.h`
     - Recompile and upload

## Configuration

### Temperature Thresholds
Edit in `include/config.h`:
```cpp
constexpr float HOT_THRESHOLD_C = 26.0;    // Close above this
constexpr float COLD_THRESHOLD_C = 20.0;   // Open below this
```

### Node ID
Edit in `src/main.cpp` (setup function):
```cpp
uint8_t assignedNodeId = 0x06;  // Change to desired node ID
```

### Servo Angles
Edit in `include/pins.h`:
```cpp
constexpr int SERVO_OPEN_ANGLE = 0;      // Fully open
constexpr int SERVO_CLOSED_ANGLE = 90;   // Fully closed
constexpr int SERVO_SETTLE_MS = 700;     // Time to settle before TX
```

### LoRa Frequency & Settings
Edit in `src/main.cpp` (initLoRa function). Current defaults:
- Frequency: 867 MHz (EU ISM band)
- Spreading Factor: SF7
- Bandwidth: 125 kHz
- Power: 14 dBm

## Packet Format

All packets follow the network protocol:

```
[Header (5 bytes)] | [Encrypted Payload] | [MIC (4 bytes)] | [CRC (1 byte)]

Header:
  [Src ID] [Dst ID] [Type] [Counter High] [Counter Low]

Encryption: AES-128 CCM with shared key
Nonce: [Src ID] [Counter High] [Counter Low] [zeros]
```

## Security

- **Encryption Key**: Shared AES-128 key (hardcoded, same as other nodes)
- **Authentication**: 4-byte MIC (CCM mode)
- **Replay Protection**: Packet counter incremented on each transmission
- **CRC**: Simple CRC8 over encrypted payload for corruption detection

## Troubleshooting

### Servo doesn't move
- Check GPIO 13 connectivity
- Verify external 5V power supply is connected and GND is common
- Check servo control signal with oscilloscope (should see PWM)
- Try manual servo test: upload a simple servo sweep sketch

### DHT11 reads NaN
- Verify GPIO 4 connection
- Check DHT11 pull-up resistor (typically 4.7k–10k between data and 3.3V)
- Allow 1–2 seconds after power-on before reading
- Reduce DHT11 read frequency (sensor has ~2 sec min. interval)

### LoRa packets not received at gateway
- Verify RN2483 UART connections (RX/TX on correct pins)
- Check baud rate: 57600
- Confirm gateway is in receive mode
- Check packet counter increment in logs (should go 0x0000 → 0x0001 → ...)

### MIC verification failures on gateway
- Ensure shared AES key is identical on node and gateway
- Check that nonce construction is correct (src_id, counter bytes)
- Verify packet counter doesn't overflow unexpectedly

## Future Enhancements

1. **TDMA Sync**: Implement beacon listening and TDMA slot synchronization (currently listens indefinitely)
2. **Deep Sleep**: Add RTC memory for persistent state across deep sleeps
3. **Bidirectional Commands**: Enable server to send open/close/toggle commands
4. **Sensor Validation**: Add hysteresis within temp range to prevent jitter
5. **Status Reporting**: Send periodic heartbeat even without state change
6. **Battery Monitoring**: Add battery voltage ADC reading and low-battery alert

## References

- [LoRa-sensor-module-agile](../lora-sensor-module-agile) — Similar sensor node implementation
- [Light-node-agile](../light-node-agile) — Actuator node with continuous listening
- [Server](../server) — Gateway packet handling and Blynk integration
- [RN2483 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/40001811A.pdf)
- [DHT11 Datasheet](https://www.mouser.com/datasheet/2/758/DHT11-307340-11_EN.pdf)
- [SG90 Servo Datasheet](https://www.towerhobbies.com/cgi-bin/wti0001p?&I=LXDC3&P=0)
