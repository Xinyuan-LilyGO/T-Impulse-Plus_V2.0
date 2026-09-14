# V2 GNSS Shared-Control-Net Diagnostic

## Purpose

On V2, `P0.24` is the `GPS_EN` control net and the legacy `GPS_1PPS` name refers to the same physical net. It is not an independently verifiable PPS input. This example follows the `v2_bringup` order, drives safe HIGH/LOW control levels, and reports that `GPS_VDD` must be measured externally. It does not initialize GNSS UART, a PPS interrupt, or another peripheral.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_gnss_pps_test`, observe the P0.24 OFF/ON control phases, and measure `GPS_VDD` with a multimeter.

## Expected behavior

- The log shows the `P0.24/GPS_EN` HIGH and LOW output phases.
- OFF and ON transition counters increase once, and the log explicitly asks for an external `GPS_VDD` measurement.
- The example does not configure P0.24 as an input, poll an edge, or attach a PPS interrupt.

## Failure diagnosis

- P0.24 read-back is wrong: inspect the `GPS_EN` control net, power, ground, and soldering.
- `GPS_VDD` is wrong: inspect the external supply, load switch, and measurement point; a software GPIO state is not a voltage measurement.
- A PPS waveform is required: first confirm that the schematic exposes an independent PPS network. This shared P0.24 example cannot establish that fact.

## Notes

Do not configure `P0.24` as legacy BOOT or independent PPS. This example is limited to the `GPS_EN` control output.
