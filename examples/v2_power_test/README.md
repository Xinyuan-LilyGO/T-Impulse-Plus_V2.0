# V2 RT9080 Rail Test

## Purpose

This focused example checks the V2 `RT9080_EN=P0.19` control output. It applies the same high-low-high sequence used by `v2_bringup`, with a `100 ms` settling interval after each transition. It does not initialize the battery ADC, I2C, SPI, UART, BLE, motor, touch input, screen, GNSS, or LoRa.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_power_test`, and record the read-back level. Measure `VDD3V3` and other rails with a multimeter; the software log only reports the GPIO state.

## Expected behavior

- The log identifies `RT9080_EN=P0.19` and reports a high read-back after the high-low-high sequence.
- The result line reports `PASS` when the GPIO reads high.
- A heartbeat continues once per second without starting any unrelated peripheral.

## Failure diagnosis

- A low read-back level: inspect the `P0.19` routing, regulator enable network, power supply, and ground.
- A correct GPIO level but missing `VDD3V3`: inspect the RT9080 device and surrounding power circuit; do not treat the software read-back as a voltage measurement.
- Unexpected bus or peripheral activity: verify that the selected PlatformIO environment is `v2_power_test` and that no other sketch is running.

## Notes

The rail remains enabled while the sketch is running. Disconnect power after testing if the board should not remain powered.
