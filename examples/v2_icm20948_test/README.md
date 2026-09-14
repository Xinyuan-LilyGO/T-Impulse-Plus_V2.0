# V2 ICM20948 Sensor Test

## Purpose

This example uses main I2C `SDA=P1.08`, `SCL=P0.11`, and address `0x69` to initialize the accelerometer, gyroscope, and AK09916 magnetometer. It prints raw and converted values, angles, temperature, and the IMU interrupt level on `P0.07`.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_icm20948_test`, hold the board still for one sample, and then rotate each axis slowly while comparing successive samples.

## Expected behavior

- AG initialization succeeds; magnetometer initialization also succeeds when the device is present.
- When still, at least one acceleration axis is near `+1g` or `-1g`; gyroscope values are near zero with normal noise.
- Slow rotation changes acceleration, gyroscope, angles, and magnetometer values continuously.
- Temperature output continues and `P0.07` does not reset the MCU.

## Failure diagnosis

- `0x69` does not acknowledge or AG initialization fails: inspect IMU power, main I2C, and address configuration.
- Saturated, frozen, or rotation-independent values: inspect soldering and device orientation.
- Only the magnetometer fails: separate auxiliary magnetometer wiring issues from environmental magnetic interference.

## Notes

Hold the board level and still before rotating X/Y/Z slowly. Do not interpret normal magnetic-field changes as an acceleration fault.
