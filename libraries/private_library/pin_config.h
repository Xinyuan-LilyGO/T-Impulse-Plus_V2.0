/*
 * @Description: None
 * @Author: LILYGO
 * @Date: 2024-12-06 14:37:43
 * @LastEditTime: 2026-05-05 13:50:21
 * @License: GPL 3.0
 */
#pragma once

#define _PINNUM(port, pin) ((port) * 32 + (pin))

// T-Impulse Plus hardware revision used by this worktree.
#define T_IMPULSE_PLUS_HARDWARE_VERSION 2

// V2 uses P0.24 as the GNSS power-control net. GPS_1PPS is kept as a legacy
// alias for the same physical net; it is not an independent PPS input.
#define GPS_POWER_CONTROL_AVAILABLE 1

// V2 PCB confirmation: LoRa DIO1 and the main I2C SCL are the same physical
// net. Keep LoRa disabled while the main I2C bus is in use.
#define V2_LORA_DIO1_I2C_SCL_UNVERIFIED 0
#define V2_LORA_DIO1_I2C_SCL_CONFLICT 1
#define V2_LORA_DIO1_CONFIRMED 0
#define V2_TTP223_Q_I2C_SCL_UNVERIFIED 0
#define V2_SCREEN_TTP223_Q_CONFLICT 0
#define V2_TTP223_KEY_CONFIRMED 0
#define V2_BOOT_PIN_GPS_PPS_UNVERIFIED 1
#define V2_BOOT_PIN_CONFIRMED 0
#define V2_SCREEN_I2C_CONFIRMED 1

// IIC
#define IIC_SDA_1 _PINNUM(1, 8)
#define IIC_SCL_1 _PINNUM(0, 11)
#define IIC_SDA_2 _PINNUM(1, 6)
#define IIC_SCL_2 _PINNUM(1, 4)

// ZD25WQ32CEIGR SPI
#define ZD25WQ32C_CS _PINNUM(0, 12)
#define ZD25WQ32C_SCLK _PINNUM(0, 4)
#define ZD25WQ32C_MOSI _PINNUM(0, 6)
#define ZD25WQ32C_MISO _PINNUM(1, 9)
#define ZD25WQ32C_IO0 _PINNUM(0, 6)
#define ZD25WQ32C_IO1 _PINNUM(1, 9)
#define ZD25WQ32C_IO2 _PINNUM(0, 8)
#define ZD25WQ32C_IO3 _PINNUM(0, 26)

// SSD1315
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define SCREEN_RST -1
#define SCREEN_SDA IIC_SDA_2
#define SCREEN_SCL IIC_SCL_2

// Lora S62F(SX1262)
#define SX1262_CS _PINNUM(0, 29)
#define SX1262_RST _PINNUM(0, 3)
#define SX1262_SCLK _PINNUM(1, 14)
#define SX1262_MOSI _PINNUM(0, 28)
#define SX1262_MISO _PINNUM(0, 30)
#define SX1262_BUSY _PINNUM(1, 12)
#define SX1262_INT _PINNUM(0, 11)
#define SX1262_DIO1 _PINNUM(0, 11)
#define SX1262_DIO2 _PINNUM(0, 31)

// V2 uses AcSiP RF switch control mode A.
#define SX1262_RF_VC1 _PINNUM(1, 13)
#define SX1262_RF_VC2 _PINNUM(1, 10)

// The S62F uses the built-in 32 MHz TCXO, powered at 3.0 V through SX1262
// DIO3. VREG and DCC_SW are connected through a 15 uH inductor, so use the
// SX1262 DC-DC regulator mode instead of forcing pure LDO mode.
#define SX1262_TCXO_VOLTAGE 3.0
#define SX1262_USE_REGULATOR_LDO false

// Battery
// V2 schematic: P0.17 drives Q4/HSST3139 to connect the battery divider.
#define BATTERY_MEASUREMENT_CONTROL _PINNUM(0, 17)
#define BATTERY_ADC_DATA _PINNUM(0, 5)

// RT9080
#define RT9080_EN _PINNUM(0, 19)

// GPS
// The names follow the GNSS module side. Serial2.setPins() receives the
// module TX net first (MCU RX), then the module RX net (MCU TX).
#define GPS_UART_TX _PINNUM(0, 2)
#define GPS_UART_RX _PINNUM(1, 15)
#define GPS_1PPS _PINNUM(0, 24)
#define GPS_EN _PINNUM(0, 24)
// ICM20948
#define ICM20948_ADDRESS 0x69
#define ICM20948_SDA IIC_SDA_1
#define ICM20948_SCL IIC_SCL_1
#define ICM20948_INT _PINNUM(0, 7)

// TTP223
#define TTP223_KEY _PINNUM(0, 15)
// SGM41562
#define SGM41562_ADDRESS 0x03
#define SGM41562_SDA IIC_SDA_1
#define SGM41562_SCL IIC_SCL_1
#define SGM41562_INT _PINNUM(0, 16)

// vibrate
#define VIBRATION_MOTOR_DATA _PINNUM(0, 22)
