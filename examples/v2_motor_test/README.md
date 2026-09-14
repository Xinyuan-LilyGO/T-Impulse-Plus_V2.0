# V2 Vibration Motor Test

## Purpose

This example drives only `VIBRATION_MOTOR_DATA=P0.22`. Because the motor path is active HIGH, it generates bounded `50 ms`, `100 ms`, and `150 ms` pulses.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_motor_test`, observe the three pulse start/end messages, and measure the motor-terminal voltage during each pulse.

## Expected behavior

- `P0.22` starts LOW after power-up.
- Three pulse pairs appear with durations of approximately `50 ms`, `100 ms`, and `150 ms`.
- Each pulse produces one vibration, and P0.22 remains LOW afterward.
- The supply has no unexpected sag, overcurrent, heating, or MCU reset.

## Failure diagnosis

- No vibration: measure P0.22 and motor-terminal voltage during a pulse, then inspect the motor, driver transistor, and supply.
- P0.22 does not return LOW: remove power immediately and inspect for a GPIO short or driver fault.
- Reset, heating, or high current: stop the test and do not extend the pulse duration.

## Notes

The maximum pulse is limited to `150 ms`; this example is not intended for continuous motor drive.
