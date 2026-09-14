# V2 Main I2C Line and Scan Test

## Purpose

This example owns main I2C `SDA=P1.08` and `SCL=P0.11`, scans addresses `0x03..0x77`, and records error addresses. `P0.11` is also SX1262 `DIO1`, so LoRa is not initialized during this test.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_main_i2c_test`, and record line levels, device count, error counts, and bus-release output.

## Expected behavior

- Both lines read HIGH with internal pull-ups and report `idle=yes`.
- The scan normally finds `SGM41562=0x03` and `ICM20948=0x69` when both devices are fitted.
- The log reports device count, probe count, each I2C error class, and first/last error addresses.
- The test ends with `bus_released=yes`.

## Failure diagnosis

- SDA or SCL is LOW: the example skips scanning. Check pull-ups, device power, shorts, and soldering.
- No `0x03`: inspect the charger and address network. No `0x69`: inspect IMU power, address, and main-I2C routing.
- Many changing error codes: record the first/last addresses and power conditions, and do not run LoRa at the same time.

## Notes

This example generates SCL activity on main I2C and cannot run in parallel with a LoRa example that assigns `DIO1=P0.11`.
