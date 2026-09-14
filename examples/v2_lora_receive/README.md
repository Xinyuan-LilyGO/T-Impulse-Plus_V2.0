# V2 SX1262 LoRa Receiver

## Purpose

This standalone example diagnoses SX1262 receive mode. It ends `Wire`, releases main-I2C `SDA=P1.08` and `SCL=P0.11`, and then starts the radio on `NRF_SPIM3`. It does not initialize the charger, IMU, or other main-I2C device.

The radio uses `868.0 MHz`, `125 kHz`, `SF10`, coding rate `4/6`, sync word `0xAB`, `22 dBm`, preamble `15`, CRC disabled, `3.0 V` TCXO, and the DC-DC regulator. The V2 radio pins are CS=`P0.29`, RST=`P0.03`, SCLK=`P1.14`, MOSI=`P0.28`, MISO=`P0.30`, BUSY=`P1.12`, DIO1=`P0.11`, DIO2=`P0.31`, RF_VC1=`P1.13`, and RF_VC2=`P1.10`.

## Serial test

Open a serial monitor at `115200`, flash and reset this example, and use another compatible LoRa device as the sender. Both radios must use identical settings. Use a suitable antenna or the required `50 ohm` load.

## Expected behavior

- Startup shows `Wire` and `P0.11` release before SX1262 initialization and receive mode.
- Packets report the event source (`DIO1` or `SPI-poll`), length, raw hexadecimal bytes, RSSI, SNR, and frequency error.
- After each receive event, `startReceive()` runs again and the log reports that the receive listener was restored.
- `DIO1_edges` counts DIO1 callback events while `irq` reports the SPI-polled SX1262 state.

## Failure diagnosis

- Initialization failure: check V2 radio pins, power, reset, BUSY, CS, TCXO, and DC-DC settings.
- IRQ reports a packet but DIO1 has no edges: inspect the shared `DIO1=P0.11` and main-I2C SCL network. Do not run a main-I2C example at the same time.
- No packets: verify `868.0 MHz`, `125 kHz`, `SF10`, `CR4/6`, sync `0xAB`, preamble `15`, CRC disabled, and antenna connections on both ends.
- `RADIOLIB_ERR_SPI_CMD_TIMEOUT` or similar errors: inspect the SPI lines, BUSY, CS, power, and module connection.

## Notes

The receive buffer stores at most `255` bytes. Initialization and recovery failures include a RadioLib numeric error code and release the callback, SPI, and radio pins.
