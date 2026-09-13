# Working Principle

1. The Arduino Nano reads LOW, MEDIUM, and HIGH probes for both the channel and field.
2. The Nano converts each set of probes into a level value: 0, 1, or 2.
3. The Nano sends the levels to the ESP32 at 9600 baud.
4. The ESP32 calculates rising, falling, or stable trends.
5. In automatic mode, high field water triggers drainage through the servo gate.
6. A falling field level while the gate is open indicates successful drainage.
7. A rising field level with a lower channel level can indicate possible rain/runoff.
8. The ESP32 presents system information on the OLED and web interface.
