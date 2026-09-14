# V2 GNSS UART Test

## Purpose

This example uses TinyGPSPlus to parse GNSS NMEA data and report position, date, and UTC time.

The V2 nets are:

- Module TX to MCU RX: `P0.02` (Arduino pin `2`)
- Module RX from MCU TX: `P1.15` (Arduino pin `47`)
- `GPS_EN` control net: `P0.24` (Arduino pin `24`), shared with the legacy `GPS_1PPS` name

`Serial2` uses `38400 8N1`. The example holds `GPS_EN` HIGH while assigning the UART pins and starting `Serial2`, then drives it LOW before receiving data. `GPS_VDD` is a hardware rail and must be measured externally.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_gnss_uart_test`, place the GNSS antenna outdoors or in an open area, and wait for data.

## Expected behavior

- Startup identifies the V2 UART pins and reports the HIGH-to-LOW `GPS_EN` sequence.
- After NMEA bytes arrive, the log prints raw lines, TinyGPSPlus position/date/UTC results, and counters.
- Indoor operation can produce NMEA data without a valid fix. A missing fix does not prove that UART is damaged.
- `P0.24` remains a `GPS_EN` control net and is never configured as independent PPS or legacy BOOT.

## Failure diagnosis

- `No bytes received`: check external `GPS_VDD`, ground, module TX to `P0.02`, UART levels, and the `38400` baud rate.
- The status remains `invalid`: confirm that NMEA bytes are arriving, then wait outdoors for a fix.
- Garbled time or position: check module TX/RX direction, baud rate, and logic levels.
