# V2 SX1262 LoRa Transmitter

## Purpose

This standalone example validates SX1262 transmission on the V2 board. It ends `Wire`, releases main-I2C `SDA=P1.08` and `SCL=P0.11`, and then starts the radio on `NRF_SPIM3`. It does not initialize the charger, IMU, screen, GNSS, or other main-I2C devices.

The radio uses `868.0 MHz`, `125 kHz`, `SF10`, coding rate `4/6`, sync word `0xAB`, `22 dBm`, preamble `15`, CRC disabled, `3.0 V` TCXO, and the DC-DC regulator. The V2 radio pins are CS=`P0.29`, RST=`P0.03`, SCLK=`P1.14`, MOSI=`P0.28`, MISO=`P0.30`, BUSY=`P1.12`, DIO1=`P0.11`, DIO2=`P0.31`, RF_VC1=`P1.13`, and RF_VC2=`P1.10`.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_lora_transmit`, and watch the transmitter logs. The example sends an ASCII payload every five seconds. Use `v2_lora_receive` on another board with the same radio settings to verify the link.

Use a suitable antenna or the required `50 ohm` load during RF testing. Do not connect the transmitter output directly to a receiver input.

## Expected behavior

- The log shows the serial port, the `RT9080_EN=P0.19` high-low-high sequence, and main-I2C release before `NRF_SPIM3` starts.
- SX1262 initialization succeeds with the parameters listed above.
- A payload such as `T-Impulse-Plus V2 TX seq=12 uptime_ms=60000` is sent every five seconds.
- Each successful transmission reports its sequence number. The sequence increments even if a transmission attempt fails.

## Failure diagnosis

- Initialization or transmission failure prints the stage and the RadioLib error code, then clears the DIO1 callback, ends SPI, and releases the radio pins.
- No receiver packet: verify both radios use the same frequency, bandwidth, spreading factor, coding rate, sync word, preamble, CRC setting, and antenna/load.
- SPI or BUSY errors: inspect the V2 CS, SCLK, MOSI, MISO, BUSY, reset, and power connections.
- Main-I2C failures in another example: run this transmitter separately because `DIO1=P0.11` shares the main-I2C SCL net.

## Notes

This is an intentional transmit-only example. It does not start receive mode, and it keeps the main-I2C controller stopped for the lifetime of the sketch.
