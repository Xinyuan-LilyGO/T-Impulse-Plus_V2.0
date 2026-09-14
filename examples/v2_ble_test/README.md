# V2 BLE UART Test

## Purpose

This example uses the nRF52840 internal Bluetooth radio and the Nordic UART Service. After a connection it sends a MAC and uptime heartbeat once per second and forwards data in both directions between USB serial and BLE UART.

## Serial test

Open a serial monitor at `115200`, flash and reset `v2_ble_test`, and wait for the advertising message. Connect with a BLE debugging tool and send short text in both directions.

## Expected behavior

- Startup reports the serial port, rail sequence, and successful BLE initialization.
- A phone or BLE tool discovers `T-Impulse-Plus-V2` and connects to the UART service.
- Connection and disconnection callbacks report the peer name and reason code.
- BLE receives a V2 heartbeat once per second, and USB/BLE text is forwarded in both directions.

## Failure diagnosis

- The device is not discovered: check the 3.3 V rail, antenna area, and that the tool scans BLE rather than classic Bluetooth serial.
- The device connects but no data moves: select the Nordic UART Service and its RX/TX characteristics.
- A reset or power fault occurs after connection: check supply current and memory configuration, and do not run another large example at the same time.

## Notes

Only BLE and `RT9080_EN` are initialized. The example does not start the screen, main I2C, Flash, GNSS, LoRa, or battery ADC.
