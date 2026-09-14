# V2 QSPI Flash Read-Only Test

## Purpose

This example follows the QSPI Flash transport used by `v2_bringup`: `CS=P0.12`, `SCLK=P0.04`, `IO0=P0.06`, `IO1=P1.09`, `IO2=P0.08`, and `IO3=P0.26`. It reads the JEDEC ID and capacity only, then releases all six Flash pins.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_flash_test`, and record the QSPI initialization, JEDEC ID, capacity, and `pins_released` messages.

## Expected behavior

- Startup prints the V2 QSPI CS, clock, and IO0-IO3 pins.
- The Flash lifecycle performs initialization, JEDEC ID read, and capacity read only.
- Typical output is `JEDEC=0xBA4016` and about `4194304 bytes/4096 kbytes`.
- The test calls `flash.end()` and reports `pins_released=yes` without manual wake, sleep, erase, or write commands.

## Failure diagnosis

- Initialization fails: check `VDD3V3`, all six QSPI pins, Flash soldering, and the part number.
- JEDEC ID or capacity is unexpected: verify the actual device and power before changing the clock.
- Pins are not released: inspect the library lifecycle and reset behavior so the Flash does not affect another peripheral.

## Notes

No erase, write, formatting, or filesystem API is used. This example is intended to confirm Flash communication before any data operation is attempted.
