# V2 Battery ADC Statistics Test

## Purpose

This focused example validates the V2 battery measurement path: `BATTERY_MEASUREMENT_CONTROL=P0.17` and `BATTERY_ADC_DATA=P0.05`. The control net drives the battery-divider switch. Each LOW/HIGH state is sampled 16 times using the 3.0 V internal reference, 12-bit ADC, and 2.0 divider ratio.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_battery_test`, and watch the 16-sample statistics for both control states. Measure the battery terminal voltage with a multimeter at the same time.

## Expected behavior

- Startup reports the ADC reference, resolution, and V2 pin map.
- Every three seconds the example alternates the divider control state and prints minimum, maximum, and average ADC values.
- The ADC is not permanently `0` or `4095`, and repeated samples in one state are reasonably close.
- The calculated battery voltage is consistent with the multimeter reading within divider and ADC tolerances.

## Failure diagnosis

- The control read-back does not change with LOW/HIGH: inspect the `P0.17` routing and the Q4 divider-switch circuit.
- Both control states produce identical ADC values: inspect the switching transistor, divider resistors, and `P0.05` network.
- Saturated or unstable ADC values: check the battery voltage range, reference configuration, ground, and wiring.

## Notes

`P0.24` is the V2 `GPS_EN` control net. It is not used by this example and must not be treated as a BOOT or independent PPS input.
