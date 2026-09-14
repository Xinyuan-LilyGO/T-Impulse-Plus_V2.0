# V2 Integrated Bring-Up Reference

## Purpose

`v2_bringup` establishes a board-level baseline by checking both I2C buses, V2 QSPI Flash, GNSS UART, and the board GPIO state. It is a reference workflow, not a replacement for the focused examples.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_bringup`, and follow the home-screen workflow. Startup first applies the `RT9080_EN=P0.19` high-low-high sequence, then initializes the screen, BLE, charger, QSPI Flash, and IMU. GNSS and LoRa resources are switched only when their windows are entered.

## Expected behavior

- Startup prints the complete V2 pin map.
- Main I2C `P1.08/P0.11` is idle and normally finds `0x03` and `0x69`.
- Screen I2C uses `P1.06/P1.04` and normally finds `0x3C` when the lines are idle.
- Flash uses QSPI with `CS=P0.12`, `SCLK=P0.04`, `IO0=P0.06`, `IO1=P1.09`, `IO2=P0.08`, and `IO3=P0.26`; typical output is `JEDEC=0xBA4016` and about `4096 kbytes`.
- GNSS uses `38400` baud and module TX/RX nets `P0.02/P1.15`; bytes and NMEA results increase when the module and external `GPS_VDD` are powered.
- `P0.24` is diagnosed only as the `GPS_EN` control net. Use a multimeter to check `GPS_VDD`; the GPIO level cannot prove an independent PPS signal.

## Failure diagnosis

- An I2C line is LOW: inspect that bus and its device power first; the example skips scanning when a line is held LOW.
- Flash ID or capacity is unexpected: inspect all six QSPI pins, power, soldering, and the actual Flash part. This example does not erase or write Flash.
- `gnss_bytes=0`: check external `GPS_VDD`, the module TX route to `P0.02`, and ground. No indoor fix does not prove a UART failure.
- `P0.24` has the wrong level: inspect the `GPS_EN` control net and `GPS_VDD`. Do not configure this net as independent PPS or legacy BOOT.
- LoRa initializes only in the LoRa window. The program releases main I2C before assigning `DIO1=P0.11` and polls SX1262 IRQ state as a fallback for missed shared-net edges.
- Automatic home sleep switches the OLED off, releases I2C and peripherals, disables `RT9080_EN`, and waits for the touch input. A touch wake restores the active window in place.

## Notes

This example initializes multiple devices. Use the corresponding `v2_*_test` example to isolate a specific hardware fault.
