# V2 TTP223 Touch Test

## Purpose

This example reads TTP223 Q on `P0.15`. It establishes an idle baseline for one second after power-up, samples every `20 ms`, and records an event only after the level is stable for `60 ms`.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_ttp223_test`, wait for the baseline, and then perform no touch, a short touch, and a long touch while watching the press/release counters.

## Expected behavior

- Startup reports `P0.15` and the baseline statistics.
- With no touch, the raw level is stable and press/release counts do not increase.
- A short or long touch creates one `pressed` and one `released` event without debounce duplicates.
- The stable touch level differs from the idle baseline.

## Failure diagnosis

- The baseline changes without touch: inspect TTP223 power, ground, soldering, and the touch pad.
- Touching does not change the level: inspect `Q=P0.15` routing and the TTP223 output mode.
- One action creates multiple events: inspect input floating, environmental interference, and the stability interval.

## Notes

This example does not initialize the screen or any I2C bus. Keep `V2_TTP223_KEY_CONFIRMED` at `0` until the hardware is independently verified.
