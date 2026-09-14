# V2 SGM41562 Charger Test

## Purpose

This example uses main I2C `P1.08/P0.11` at address `0x03` to read the device ID, charger configuration, fault status, and chip status. The interrupt is `P0.16`. Battery ADC behavior is tested separately by `v2_battery_test`.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_sgm41562_test`, and record the device ID, configuration, IRQ, and chip-status output under a known USB and battery condition.

## Expected behavior

- SGM41562 initialization succeeds and reports the model and `device_id`.
- Charger configuration, IRQ, and chip status can be read repeatedly; charge status changes with USB and battery conditions.
- `P0.16` and fault fields are stable, and chip status matches the actual USB/battery state.
- The charger and battery show no unexpected heating.

## Failure diagnosis

- Initialization or ID read fails: check address `0x03`, main I2C, chip power, and ground.
- A fault field is set: consider USB, battery, NTC, and temperature conditions before attributing it to software.
- Readings are unstable or the board heats: remove power and inspect the charger network before repeating the test.

## Notes

The driver's `Init()` performs chip identification, register reset, and its initialization sequence. This example does not call charging, shipping-mode, or safety-timer setters.
