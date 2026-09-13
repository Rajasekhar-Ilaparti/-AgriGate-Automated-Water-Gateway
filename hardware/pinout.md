# AgriGate Pinout

## ESP32

- GPIO18: Servo
- GPIO19: Green LED
- GPIO21: Red LED
- GPIO32: OLED SDA
- GPIO22: OLED SCL
- GPIO27: Open button
- GPIO14: Close button
- GPIO33: Manual selector
- GPIO12: Auto selector
- GPIO16: Nano RX
- GPIO17: Nano TX

## Nano

- D2: Channel LOW
- D3: Channel MEDIUM
- D4: Channel HIGH
- D5: Field LOW
- D6: Field MEDIUM
- D7: Field HIGH

Nano TX is 5 V logic. Use a voltage divider before connecting it to ESP32 GPIO16.
