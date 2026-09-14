# V2 SX1262 LoRa Receive Test

## Purpose

This standalone example validates SX1262 reception on the V2 board. It ends `Wire`, releases main-I2C `SDA=P1.08` and `SCL=P0.11`, and then starts the radio on `NRF_SPIM3`. It does not initialize the charger, IMU, screen, GNSS, or other main-I2C devices.

The radio uses `868.0 MHz`, `125 kHz`, `SF10`, coding rate `4/6`, sync word `0xAB`, `22 dBm`, preamble `15`, CRC disabled, `3.0 V` TCXO, and the DC-DC regulator. The V2 radio pins are CS=`P0.29`, RST=`P0.03`, SCLK=`P1.14`, MOSI=`P0.28`, MISO=`P0.30`, BUSY=`P1.12`, DIO1=`P0.11`, DIO2=`P0.31`, RF_VC1=`P1.13`, and RF_VC2=`P1.10`.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_lora_test`, and use `v2_lora_transmit` or another compatible LoRa device as the sender. Keep both radios on the same settings and use a suitable antenna or the required `50 ohm` load.

## Expected behavior

- The log shows main-I2C release before `NRF_SPIM3` starts and then reports successful SX1262 initialization.
- Packets report their source (`DIO1` or `SPI-poll`), length, raw hexadecimal bytes, RSSI, SNR, and frequency error.
- The receiver restarts `startReceive()` after each packet or CRC event.
- `DIO1_edges` and the SPI IRQ snapshot help diagnose the shared `P0.11` network.

## Failure diagnosis

- Initialization failure prints the stage and RadioLib error code, then clears the callback, ends SPI, and releases the radio pins.
- Missing packets: verify the sender uses `868.0 MHz`, `125 kHz`, `SF10`, `CR4/6`, sync `0xAB`, preamble `15`, and CRC disabled.
- IRQ events without DIO1 edges: inspect the shared `DIO1=P0.11` and main-I2C SCL network; do not run a main-I2C example at the same time.
- SPI or BUSY errors: inspect the V2 radio pin map, power rail, reset, TCXO, and DC-DC configuration.

## Notes

This is a receive-only diagnostic. It never calls `radio.transmit()` and keeps the main-I2C controller stopped for the lifetime of the sketch.
