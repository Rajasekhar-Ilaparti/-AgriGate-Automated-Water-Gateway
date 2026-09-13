# 🌾 AgriGate – Automated Water Gateway

An intelligent automated water-management and gate-control prototype for agricultural fields, especially paddy fields.

AgriGate monitors channel and field water levels using an Arduino Nano, communicates the readings to an ESP32, and provides automatic/manual servo gate control, OLED status, web monitoring, timers, runoff inference, and event history.

## Features

- Channel and field water-level monitoring
- Automatic servo-controlled gate
- Manual and automatic modes
- ESP32 web dashboard
- OLED status display
- Water-level trend detection
- Possible rain/runoff inference
- Automatic high-water drainage
- Drainage-success/problem detection
- Gate status LEDs
- Timed gate operation
- Event/history logging
- ESP32 NVS persistent history
- Arduino Nano ↔ ESP32 serial communication

## Hardware

- Arduino Nano
- ESP32 Dev Module
- Servo motor
- 128×64 OLED
- Six water probes
- Green and red LEDs
- Open/close push buttons
- Manual/Auto selector switch
- External servo supply

## ESP32 Pinout

| Function | GPIO |
|---|---:|
| Servo | 18 |
| Green LED | 19 |
| Red LED | 21 |
| OLED SDA | 32 |
| OLED SCL | 22 |
| Open button | 27 |
| Close button | 14 |
| Manual switch | 33 |
| Auto switch | 12 |
| Nano RX | 16 |
| Nano TX | 17 |

## Arduino Nano Pinout

| Function | Pin |
|---|---:|
| Channel LOW | D2 |
| Channel MEDIUM | D3 |
| Channel HIGH | D4 |
| Field LOW | D5 |
| Field MEDIUM | D6 |
| Field HIGH | D7 |

Water probes use `INPUT_PULLUP` and active-LOW detection.

## Communication

The Nano sends:

```text
C:<channelLevel>,F:<fieldLevel>
```

Example:

```text
C:1,F:2
```

Where `0 = LOW`, `1 = MEDIUM`, `2 = HIGH`.

## Gate Positions

```text
Servo 0°   → Gate OPEN
Servo 180° → Gate CLOSED
```

## Automatic Logic

```text
Field HIGH
    ↓
Drainage required
    ↓
Open gate

Gate OPEN + Field FALLING
    ↓
Drainage successful

Field LOW
    ↓
Close gate

Field RISING + Channel LOW/MEDIUM
    ↓
Possible runoff/rain condition
```

## OLED

The OLED provides local system status and can display gate-operation events/animations.

Example:

```text
STARTING...
OPENING GATE
```

or:

```text
STARTING...
CLOSING GATE
```

## Project Status

**Prototype / Development**

## Future Improvements

- Soil-moisture sensing
- Rain sensor integration
- Water-flow measurement
- Cloud logging
- Mobile application
- MQTT
- Solar power
- Multiple field/gate support
- Weather integration
- Predictive water management

## License

Educational, research, and prototype use.
