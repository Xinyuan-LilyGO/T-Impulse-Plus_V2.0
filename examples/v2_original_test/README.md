# V2 Integrated Hardware Test

## Purpose

This example preserves the integrated home screen, long-press window switching, single-click actions, and double-click power-off flow. It follows the tested `v2_bringup` V2 initialization and resource-ownership order. Run it after the focused examples for an integrated interaction regression; use a focused example to isolate a fault.

The main V2 nets are:

- Screen I2C: `SDA=P1.06`, `SCL=P1.04`, address `0x3C`
- Main I2C: `SDA=P1.08`, `SCL=P0.11`
- Battery measurement: control `P0.17`, ADC `P0.05`
- GNSS: module TX=`P0.02`, module RX=`P1.15`, `GPS_EN=P0.24`; `GPS_VDD` is externally powered and this net is not independent PPS
- TTP223: `Q=P0.15`
- SX1262: NSS=`P0.29`, RST=`P0.03`, SCLK=`P1.14`, MOSI=`P0.28`, MISO=`P0.30`, BUSY=`P1.12`, DIO1=`P0.11`, DIO2=`P0.31`
- Flash QSPI: CS=`P0.12`, SCLK=`P0.04`, IO0=`P0.06`, IO1=`P1.09`, IO2=`P0.08`, IO3=`P0.26`

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_original_test`, and first watch the serial initialization, V2 pin map, power, screen, BLE, SGM41562, Flash, and ICM20948 messages. Then operate the windows with TTP223.

Run `v2_power_test`, `v2_screen_test`, `v2_main_i2c_test`, `v2_flash_test`, `v2_sgm41562_test`, `v2_icm20948_test`, `v2_battery_test`, `v2_ttp223_test`, the GNSS tests, `v2_ble_test`, and `v2_lora_test` before this integrated test.

## Controls

- Long press cycles through `HOME`, `FLASH_TEST`, `BATTERY_TEST`, `IMU_TEST`, `GPS_TEST`, and `LORA_TEST`.
- Single click toggles battery sampling. The LoRa window is receive-only and does not transmit.
- Double click on the home screen turns off the OLED, requests charger shipping mode, and enters no-wake System OFF. Confirm battery and charger conditions before using this action.

## Expected behavior

- Startup reports the serial port and complete V2 pin map.
- The display shows the startup screen and home screen while uptime is logged.
- BLE initializes and advertises; BLE UART and USB serial forward data in both directions after connection.
- The Flash window reports JEDEC ID and capacity. The battery window reports the `P0.17` switch state, ADC voltage, and calculated voltage.
- The IMU window reports changing attitude values. The GNSS window reports raw data or parsed results when the module is powered.
- The LoRa window releases main I2C `P0.11` before starting SPI and receive mode, then reports packet bytes, RSSI, SNR, and frequency error.

## Failure diagnosis

- No serial-ready message: check USB, MCU power, flashing setup, and reset before diagnosing a peripheral.
- Screen failure: verify `P1.06/P1.04`, `0x3C`, power, and the focused screen example.
- SGM41562 or ICM20948 failure: verify main I2C `P1.08/P0.11`, device power, and addresses. Do not initialize LoRa while main I2C is active.
- Battery values remain `0`, `4095`, or drift: inspect the `P0.17` switch, `P0.05` ADC, divider, and battery connection, then compare with a multimeter.
- GNSS has no bytes: verify external `GPS_VDD`, TX/RX direction, and `38400` baud. No indoor fix does not prove UART damage.
- LoRa is receive-only in this window. `DIO1=P0.11` shares the main-I2C SCL net, so the example releases main I2C before radio startup and cannot run both functions in parallel.
- Double-click power-off enters no-wake System OFF. Press reset or power-cycle before continuing serial debugging.

## Notes

`P0.24` remains the V2 `GPS_EN` control net and is never used as legacy BOOT or independent PPS. LoRa starts only after main-I2C release. Measure power rails and current with instruments; serial logs cannot replace those measurements.
