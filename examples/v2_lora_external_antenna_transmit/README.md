# V2 SX1262 LoRa Transmitter With SPDT Antenna Selection

## Purpose

This standalone example transmits SX1262 test packets and lets a TTP223 touch click select the board SPDT antenna path. The touch output is `Q=P0.15`; one stable press toggles the path. The SPDT control is `VCTL=P1.07`: `HIGH` selects the internal LoRa antenna and `LOW` selects the external LoRa antenna.

The example also controls the S62F internal RF switch with `RF_VC1=P1.13` and `RF_VC2=P1.10`. It ends `Wire`, releases main-I2C `SDA=P1.08` and `SCL=P0.11`, and then starts the radio on `NRF_SPIM3`.

The radio uses `868.0 MHz`, `125 kHz`, `SF10`, coding rate `4/6`, sync word `0xAB`, `22 dBm`, preamble `15`, CRC disabled, `3.0 V` TCXO, and the DC-DC regulator.

## Serial test

Open a serial monitor at `115200`, flash and reset this example, and watch the transmitter logs. The example starts on the internal antenna, then sends an ASCII payload through the selected antenna every five seconds. Touch the TTP223 once to switch to the external antenna, and touch it again to switch back. Use `v2_lora_receive` on another board with the same radio settings to verify the link.

Use a suitable internal antenna or the required `50 ohm` load during RF testing. Do not connect the transmitter output directly to a receiver input.

## Expected behavior

- Startup reports `SPDT VCTL=P1.07 level=HIGH` and initializes `TTP223 Q=P0.15` before SX1262 initialization.
- A stable TTP223 press toggles `P1.07` between `HIGH` (internal antenna) and `LOW` (external antenna) exactly once per press.
- Startup reports the `RT9080_EN=P0.19` high-low-high sequence and main-I2C release before `NRF_SPIM3` starts.
- SX1262 initialization succeeds with the parameters listed above.
- A payload containing the selected antenna name is sent every five seconds.
- Each successful transmission reports its sequence number and the observed IRQ flags.

## Failure diagnosis

- No receiver packet: verify the intended antenna path is selected, `P1.07` is HIGH for internal or LOW for external, and both radios use the same frequency, bandwidth, spreading factor, coding rate, sync word, preamble, and CRC setting.
- The path changes repeatedly while touching: verify the TTP223 output is connected to `P0.15` and that the touch surface is released between clicks. The code samples every `20 ms` and requires `60 ms` of stable state.
- Initialization or transmission failure prints the stage and the RadioLib error code, then clears the DIO1 callback, ends SPI, and releases the radio pins.
- `RADIOLIB_ERR_SPI_CMD_TIMEOUT` or similar errors: inspect the SPI lines, BUSY, CS, power, reset, TCXO, and DC-DC settings.
- Main-I2C failures in another example: run this transmitter separately because `DIO1=P0.11` shares the main-I2C SCL net.

## Notes

The SPDT remains driven at the currently selected level after radio cleanup. This example is transmit-only and does not start receive mode.
