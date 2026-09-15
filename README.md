<!--
 * @Description: T-Impulse-Plus V2 hardware debug documentation
 * @Author: LILYGO_L
 * @Date: 2023-09-11 16:13:14
 * @LastEditTime: 2026-09-14
 * @License: GPL 3.0
-->
<h1 align="center">T-Impulse-Plus V2.0</h1>

## English | [Chinese](./README_CN.md)

## Current baseline

This repository currently targets the T-Impulse-Plus V2 hardware debug
baseline. The nRF52840 remains the MCU, but the V2 pin map, peripheral
ownership, power-up sequence, and LoRa/I2C resource handoff are different from
the original V1 examples.

The integrated v2_bringup example is the tested reference for the V2
initialization order. The other v2_* examples are focused diagnostics that
initialize only the hardware under test. A successful serial log does not
replace external measurements of power rails, battery voltage, RF output, or
GNSS power.

| Baseline | Status | Scope |
| --- | --- | --- |
| V2 hardware debug examples | Current | 15 focused or integrated PlatformIO environments |

## Product information

| Product | MCU | Flash | RAM | Purchase link |
| --- | --- | --- | --- | --- |
| T-Impulse-Plus V2 | nRF52840 | 1 MB | 256 KB | N/A |

## Contents

- [Overview](#overview)
- [Hardware modules](#hardware-modules)
- [V2 hardware pin map](#v2-hardware-pin-map)
- [LoRa parameters](#lora-parameters)
- [Reference initialization order](#reference-initialization-order)
- [V2 examples](#v2-examples)
- [Setup and flashing](#setup-and-flashing)
- [Recommended validation order](#recommended-validation-order)
- [Legacy assets and project files](#legacy-assets-and-project-files)
- [FAQ](#faq)

## Overview

T-Impulse-Plus V2 is a low-power wristband based on the nRF52840. The board
includes an OLED display, SX1262 LoRa radio, MIA-M10Q GNSS module, ICM20948
inertial sensor, QSPI Flash, TTP223 touch input, SGM41562 power-management IC,
and an RT9080-controlled 3.3 V rail.

The V2 examples use English serial logs at 115200 baud. The v2_bringup sketch
keeps the original integrated-test behavior and naming where required, but
uses the V2 hardware definition and the V2 resource-ownership rules.

## Preview

<p align="center" width="100%">
    <img src="image/3.jpg" alt="T-Impulse-Plus board">
</p>

## Hardware modules

### MCU

- Chip: nRF52840
- RAM: 256 KB
- Flash: 1 MB
- [nRF52840 datasheet](https://docs.nordicsemi.com/bundle/ps_nrf52840/page/keyfeatures_html5.html)

### Display

- Type: SSD1315-compatible OLED
- Resolution: 128 x 64
- Bus: screen I2C on Wire1
- Address: 0x3C
- Pins: SDA=P1.06, SCL=P1.04
- Libraries: Adafruit_GFX and Adafruit_SSD1306
- [SSD1315 documentation](./information/SSD1315.pdf)

### LoRa

- Module: S62F
- Transceiver: SX1262
- Bus: SPI on NRF_SPIM3
- Libraries: RadioLib, Adafruit_BusIO, and Adafruit_SPIFlash
- [S62F documentation](./information/S62F.pdf)
- [S62F application note](./information/S62F_ApplicationNote_Ver_D.pdf)

The S62F uses AcSiP RF switch control mode A. RF_VC1 and RF_VC2 are direct
MCU control signals. DIO2 is a separate SX1262 signal and must not be used as
a replacement for the RF switch controls. The V2 RF switch truth table is
RF_VC1/RF_VC2 = LOW/HIGH for receive and HIGH/LOW for transmit.

The module uses a 3.0 V TCXO setting and the SX1262 DC-DC regulator mode.
These settings are part of the V2 examples and must match at both ends of a
LoRa test.

### GNSS

- Module: MIA-M10Q
- Bus: UART, 38400 8N1
- Module TX net: P0.02
- Module RX net: P1.15
- GNSS power control: GPS_EN=P0.24
- [MIA-M10Q documentation](./information/MIA-M10Q-00B.pdf)

GPS_EN is active low in the current examples: HIGH keeps the GNSS power
control disabled and the UART test drives it LOW before receiving data.
GPS_1PPS is only a legacy alias for P0.24; the V2 board does not define an
independent PPS input on this net.

### IMU

- Chip: ICM20948
- Bus: main I2C
- Address: 0x69
- Pins: SDA=P1.08, SCL=P0.11
- Interrupt: P0.07
- [ICM20948 documentation](./information/ICM20948.pdf)

### QSPI Flash

- Compatible JEDEC IDs include ZD25WQ32C (BA 60 16) and ZD25Q32D (BA 40 16)
- Bus: QSPI
- CS=P0.12, SCLK=P0.04, IO0=P0.06, IO1=P1.09, IO2=P0.08, IO3=P0.26
- Library: Adafruit_SPIFlash
- Record the JEDEC ID and capacity from v2_flash_test; no separate Flash PDF
  is included in this repository.

### Touch input

- Chip: TTP223
- Q output: P0.15
- The V2 focused test treats this as an input and applies software debounce.
  Keep the key confirmation macro disabled until the assembled board is
  independently checked.
- [TTP223 documentation](./information/TTP223-BA6-TD.pdf)

### Power and battery

- Charger/power-management IC: SGM41562
- SGM41562 address: 0x03
- SGM41562 main-I2C pins: SDA=P1.08, SCL=P0.11
- SGM41562 interrupt: P0.16
- 3.3 V rail enable: RT9080_EN=P0.19
- Battery-divider switch control: P0.17
- Battery ADC: P0.05
- [SGM41562 documentation](./information/SGMICRO-SGM41562XGTR.pdf)

## V2 hardware pin map

The authoritative software definitions are in
[libraries/private_library/pin_config.h](./libraries/private_library/pin_config.h).
The schematic is available at
[project/T-Impulse%20Plus.pdf](./project/T-Impulse%20Plus.pdf).

| Function | V2 pin or setting | Notes |
| --- | --- | --- |
| Screen I2C | SDA=P1.06, SCL=P1.04 | Wire1, address 0x3C, 128 x 64 |
| Main I2C | SDA=P1.08, SCL=P0.11 | SCL is shared with SX1262 DIO1 |
| SX1262 CS | P0.29 | |
| SX1262 reset | P0.03 | |
| SX1262 SCLK | P1.14 | NRF_SPIM3 |
| SX1262 MOSI | P0.28 | |
| SX1262 MISO | P0.30 | |
| SX1262 BUSY | P1.12 | |
| SX1262 DIO1 | P0.11 | Shared physical net with main-I2C SCL |
| SX1262 DIO2 | P0.31 | |
| SX1262 RF_VC1 | P1.13 | RF switch control |
| SX1262 RF_VC2 | P1.10 | RF switch control |
| Flash CS | P0.12 | QSPI |
| Flash SCLK | P0.04 | QSPI |
| Flash IO0 | P0.06 | QSPI |
| Flash IO1 | P1.09 | QSPI |
| Flash IO2 | P0.08 | QSPI |
| Flash IO3 | P0.26 | QSPI |
| GNSS module TX net | P0.02 | Used as the module TX side of Serial2 |
| GNSS module RX net | P1.15 | Used as the module RX side of Serial2 |
| GPS_EN | P0.24 | GPS_1PPS is only a legacy alias |
| ICM20948 interrupt | P0.07 | |
| SGM41562 interrupt | P0.16 | |
| TTP223 Q | P0.15 | |
| Vibration motor | P0.22 | Active-high pulse output |
| RT9080 enable | P0.19 | |
| Battery control / ADC | P0.17 / P0.05 | P0.17 switches the divider |

### Shared-net restriction

P0.11 is physically both main-I2C SCL and SX1262 DIO1. Main I2C and LoRa
cannot safely operate in parallel on this board. A LoRa-focused example must
call Wire.end(), release P1.08 and P0.11, and then start the radio SPI bus.
When the radio window ends, the radio callback, SPI peripheral, and radio pins
are released before main I2C is restored.

## LoRa parameters

Use identical parameters on the transmitter and receiver:

| Parameter | V2 value |
| --- | --- |
| Frequency | 868.0 MHz |
| Bandwidth | 125 kHz |
| Spreading factor | SF10 |
| Coding rate | 4/6 |
| Sync word | 0xAB |
| Output power | 22 dBm |
| Preamble | 15 symbols |
| CRC | Disabled |
| TCXO | 3.0 V |
| Regulator | DC-DC |

## Reference initialization order

v2_bringup is the reference for the real V2 startup and peripheral handoff
order:

1. Start Serial at 115200.
2. Enable RT9080_EN=P0.19 with HIGH -> LOW -> HIGH transitions and about
   100 ms between transitions.
3. Configure screen Wire1 on P1.06/P1.04 and initialize the display at 0x3C.
   Draw the startup screen and wait about one second.
4. Wait for native USB CDC for a bounded interval, then print the version
   banner and diagnostics.
5. Set TTP223 P0.15 as an input, keep GPS_EN=P0.24 HIGH, and drive the motor
   P0.22 LOW.
6. Initialize BLE, then initialize SGM41562.
7. Access QSPI Flash at 32 MHz: begin transport, send 0xAB to exit deep
   sleep, begin again to read JEDEC ID/capacity, send 0xB9, call flash.end(),
   and release all six QSPI pins.
8. Configure main I2C on P1.08/P0.11 and initialize ICM20948 at 0x69. The
   integrated setup then puts the IMU to sleep and releases its pins as
   required by the current application state.
9. Do not initialize LoRa during normal startup. In the LoRa window, end
   main I2C and release P0.11 before starting NRF_SPIM3 and SX1262.
10. In the GNSS window, configure Serial2 on P0.02/P1.15 at 38400 baud and
    drive GPS_EN LOW before reading GNSS data. On exit, stop Serial2 and
    return GPS_EN HIGH.

## V2 examples

Every current example directory contains its own README.md with serial
procedure, expected behavior, and failure diagnosis.

| Example | Purpose |
| --- | --- |
| [v2_battery_test](./examples/v2_battery_test) | 16-sample battery-divider ADC statistics for each P0.17 LOW/HIGH state |
| [v2_ble_test](./examples/v2_ble_test) | nRF52840 internal BLE Nordic UART Service test |
| [v2_bringup](./examples/v2_bringup) | Tested integrated V2 startup-order and board diagnostic reference |
| [v2_flash_test](./examples/v2_flash_test) | Read-only QSPI Flash JEDEC ID and capacity test |
| [v2_gnss_pps_test](./examples/v2_gnss_pps_test) | GPS_EN/P0.24 control-net test; no independent PPS measurement |
| [v2_gnss_uart_test](./examples/v2_gnss_uart_test) | 38400-baud GNSS UART and TinyGPSPlus NMEA test |
| [v2_icm20948_test](./examples/v2_icm20948_test) | ICM20948 accelerometer, gyroscope, and magnetometer test |
| [v2_lora_receive](./examples/v2_lora_receive) | Standalone fixed-parameter SX1262 receive example |
| [v2_lora_transmit](./examples/v2_lora_transmit) | Standalone fixed-parameter SX1262 transmitter; sends every five seconds |
| [v2_main_i2c_test](./examples/v2_main_i2c_test) | Main-I2C line state, address scan, and error statistics |
| [v2_motor_test](./examples/v2_motor_test) | Bounded 50 ms, 100 ms, and 150 ms motor pulses |
| [v2_original_test](./examples/v2_original_test) | V1-compatible integrated menu and V2 peripheral regression test |
| [v2_screen_test](./examples/v2_screen_test) | Screen I2C line, address, and 128 x 64 display test |
| [v2_sgm41562_test](./examples/v2_sgm41562_test) | SGM41562 device ID, configuration, fault, and status test |
| [v2_ttp223_test](./examples/v2_ttp223_test) | TTP223 P0.15 baseline and debounced input test |

The v2_original_test LoRa window is receive-only. Use v2_lora_transmit with
v2_lora_receive or v2_lora_test on another board to verify a two-device RF
link.

## Setup and flashing

### PlatformIO

The repository includes the custom LilyGo T-Impulse Plus nRF52840 board
definition and registers one PlatformIO environment for each V2 example.
Use an explicit environment name so the selected example is mapped to its
matching examples/<name> directory:

    pio run -e v2_bringup
    pio run -e v2_bringup -t upload
    pio device monitor -e v2_bringup -b 115200

Replace v2_bringup with any name in the V2 examples table. The helper script
tools/platformio_select_example.py prevents the default source directory
from silently replacing the selected environment.

Install Visual Studio Code and the PlatformIO IDE extension, open this
repository as the project folder, and select the included
LilyGo T-Impulse Plus nRF52840 board. If the local board or framework setup
needs repair, run the repository setup script:

    python "tool/win10 vscode platformio start/t_impulse_plus_setup.py"

The repository also includes static V2 contract checks. They inspect source
and configuration only:

    powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\tests\verify_v2_hardware_config.ps1

### Arduino IDE

1. Install Arduino IDE and the Adafruit nRF52 board package.
2. Run tool/win10 arduino ide start/t_impulse_plus_arduino_setup.py to install
   the T-Impulse-Plus board variant and the required compiler library flags.
3. Open an .ino file inside the selected examples/v2_* directory.
4. Select LilyGo T-Impulse Plus nRF52840 and the correct USB port.
5. Use a serial monitor at 115200 baud.
6. To enter bootloader download mode, press and release RST, wait one second,
   then press and release RST again. A new USB drive indicates bootloader mode.

### J-Link

The J-Link/SWD wiring image is [image/12.jpg](./image/12.jpg). Use
nRF Connect for Desktop Programmer with the bootloader and firmware assets
only when a prebuilt image is specifically required. The V2 examples are
normally built and uploaded through PlatformIO or Arduino IDE.

## Recommended validation order

1. Run v2_bringup first to confirm the tested integrated startup markers.
2. Run v2_power_test and measure VDD3V3 externally.
3. Run v2_screen_test and confirm the 0x3C display result.
4. Run v2_flash_test and record JEDEC ID and capacity.
5. Run v2_main_i2c_test, v2_sgm41562_test, and v2_icm20948_test.
6. Run v2_battery_test and compare the calculated voltage with a multimeter.
7. Run v2_motor_test and v2_ttp223_test separately.
8. Run v2_gnss_uart_test, then v2_gnss_pps_test for GPS_EN control levels.
9. Run v2_ble_test.
10. Run v2_lora_receive or v2_lora_test, then run v2_lora_transmit on a
    separate board with matching parameters.
11. Run v2_original_test for the integrated menu and resource-handoff
    regression.

### LoRa transmit and receive test

Flash v2_lora_receive to one board and v2_lora_transmit to another board.
Start the receiver first. Both boards must use the parameters in the LoRa
parameters table. The transmitter sends an ASCII test payload every five
seconds and checks TX_DONE through SPI because DIO1 is on the shared P0.11
net. The receiver reports the packet source, bytes, RSSI, SNR, and frequency
error.

Use an antenna, a suitable 50 ohm load, or an RF test fixture. Never connect
a transmitter output directly to a receiver input. Do not run a main-I2C
example in parallel with a standalone LoRa example on the same board.

## FAQ

### Why is there no serial output?

Please enable the "DTR" option in your serial assistant software.

Open the monitor at 115200 before resetting or reconnecting USB. Check USB
CDC, VBUS, MCU power, reset, and the selected PlatformIO environment. The
focused examples bound their USB wait; a late monitor connection can still
miss the first startup lines.

### Why does direct USB programming fail?

Press and release RST, wait one second, and press and release RST again. When
the new USB drive appears, select the correct port and upload again.

## Battery Life Estimation
Calculate battery life given the device's average current consumption and battery capacity. Below are examples of battery life calculation for batteries with different energy capacities:
Example 1:
Average current consumption of the device: 20uA
Battery capacity: 220mAh (standard CR2032 coin cell battery)
Battery life: 0.22Ah/0.00002A=11000hours=458days
