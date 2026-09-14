# V2 Screen I2C Test

## Purpose

This example owns the screen I2C bus and tests an SSD1315-compatible controller at `128x64`, address `0x3C`. The V2 screen pins are `SDA=P1.06` and `SCL=P1.04`. It does not initialize main I2C, TTP223, LoRa, or GNSS.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_screen_test`, record the line check, address response, initialization, and pattern-write messages, and observe the display.

## Expected behavior

- Startup prints the screen pin map and line check.
- Screen SDA/SCL read HIGH with internal pull-ups and report `idle=yes`.
- `0x3C` acknowledges. If the address jumper selects the alternative, `0x3D` is accepted for diagnosis.
- SSD1315 initialization succeeds and the example sends a white-screen fill followed by a test pattern.
- The panel briefly shows a white screen, then a border, diagonals, and `V2 SCREEN TEST`.

## Failure diagnosis

- SDA or SCL is LOW: scanning is skipped. Inspect screen VDD/GND, pull-ups, soldering, and `P1.06/P1.04` routing.
- Neither `0x3C` nor `0x3D` acknowledges: inspect screen power, address jumper, and I2C direction.
- An address acknowledges but no image appears: inspect the panel, glass connection, controller type, and initialization conditions.

## Notes

TTP223 Q is `P0.15` on V2 and is separate from the screen I2C bus. This example does not read TTP223.
