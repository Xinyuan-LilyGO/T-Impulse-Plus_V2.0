#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t I2C_CLOCK_HZ = 100000UL;
static constexpr uint8_t FIRST_I2C_ADDRESS = 0x03;
static constexpr uint8_t LAST_I2C_ADDRESS = 0x77;
static constexpr uint8_t ERROR_BUCKET_COUNT = 6;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 3000UL;

struct I2C_Scan_Result
{
    int sda_level = LOW;
    int scl_level = LOW;
    bool idle = false;
    bool attempted = false;
    bool skipped = false;
    uint16_t probes = 0;
    uint16_t devices = 0;
    uint16_t errors[ERROR_BUCKET_COUNT] = {};
    uint8_t first_error = 0xFF;
    uint8_t first_error_address = 0xFF;
    uint8_t last_error = 0xFF;
    uint8_t last_error_address = 0xFF;
};

static I2C_Scan_Result Main_I2C_Result;
static bool Main_I2C_Test_Ready = false;
static uint32_t Next_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_main_i2c_test] Serial ready");
    Serial.flush();
}

static void Enable_3V3_Rail()
{
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
    digitalWrite(RT9080_EN, LOW);
    delay(100);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
}

static void Release_Main_I2C()
{
    Wire.end();
    pinMode(IIC_SDA_1, INPUT);
    pinMode(IIC_SCL_1, INPUT);
}

static bool Check_Main_I2C_Lines()
{
    pinMode(IIC_SDA_1, INPUT_PULLUP);
    pinMode(IIC_SCL_1, INPUT_PULLUP);
    delay(5);
    Main_I2C_Result.sda_level = digitalRead(IIC_SDA_1);
    Main_I2C_Result.scl_level = digitalRead(IIC_SCL_1);
    Main_I2C_Result.idle = (Main_I2C_Result.sda_level == HIGH) &&
                          (Main_I2C_Result.scl_level == HIGH);
    Serial.printf("[Main I2C] line_check SDA=P1.08=%d SCL=P0.11=%d idle=%s\r\n",
                  Main_I2C_Result.sda_level,
                  Main_I2C_Result.scl_level,
                  Main_I2C_Result.idle ? "yes" : "no");
    return Main_I2C_Result.idle;
}

static bool Scan_Main_I2C()
{
    Serial.println("[Stage] Main I2C diagnostic starting");
    Serial.println("[Pin map] hardware_revision=V2");
    Serial.println("[Pin map] Main I2C SDA=P1.08 SCL=P0.11");
    Serial.println("[Pin map] expected devices SGM41562=0x03 ICM20948=0x69");
    Serial.printf("[Pin map] Main-I2C SCL shares the LoRa DIO1 net conflict=%u\r\n",
                  static_cast<unsigned int>(V2_LORA_DIO1_I2C_SCL_CONFLICT));
    Serial.println("[Safety] This example owns main I2C; P0.11 is also SX1262 DIO1, so LoRa is disabled");

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");

    if (!Check_Main_I2C_Lines())
    {
        Main_I2C_Result.skipped = true;
        Serial.println("[Main I2C] Scan skipped: a line is low; check power, pull-ups, and the shared P0.11 network");
        Release_Main_I2C();
        return false;
    }

    Wire.setPins(IIC_SDA_1, IIC_SCL_1);
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);
    Main_I2C_Result.attempted = true;
    Serial.printf("[Main I2C] Wire started clock_hz=%lu address_range=0x%02X..0x%02X\r\n",
                  static_cast<unsigned long>(I2C_CLOCK_HZ),
                  static_cast<unsigned int>(FIRST_I2C_ADDRESS),
                  static_cast<unsigned int>(LAST_I2C_ADDRESS));

    for (uint8_t address = FIRST_I2C_ADDRESS;
         address <= LAST_I2C_ADDRESS; address++)
    {
        Main_I2C_Result.probes++;
        Wire.beginTransmission(address);
        const uint8_t error = Wire.endTransmission();
        if (error < ERROR_BUCKET_COUNT)
        {
            Main_I2C_Result.errors[error]++;
        }
        if (error == 0)
        {
            Main_I2C_Result.devices++;
            Serial.printf("[Main I2C] device_found address=0x%02X probe=%u\r\n",
                          static_cast<unsigned int>(address),
                          static_cast<unsigned int>(Main_I2C_Result.probes));
        }
        else
        {
            if (Main_I2C_Result.first_error == 0xFF)
            {
                Main_I2C_Result.first_error = error;
                Main_I2C_Result.first_error_address = address;
            }
            Main_I2C_Result.last_error = error;
            Main_I2C_Result.last_error_address = address;
        }
        delay(2);
    }

    Serial.printf("[Main I2C] scan_complete devices=%u probes=%u\r\n",
                  static_cast<unsigned int>(Main_I2C_Result.devices),
                  static_cast<unsigned int>(Main_I2C_Result.probes));
    Serial.printf("[Main I2C] error_counts ok=%u data_too_long=%u address_nack=%u\r\n",
                  static_cast<unsigned int>(Main_I2C_Result.errors[0]),
                  static_cast<unsigned int>(Main_I2C_Result.errors[1]),
                  static_cast<unsigned int>(Main_I2C_Result.errors[2]));
    Serial.printf("[Main I2C] error_counts data_nack=%u other=%u timeout=%u\r\n",
                  static_cast<unsigned int>(Main_I2C_Result.errors[3]),
                  static_cast<unsigned int>(Main_I2C_Result.errors[4]),
                  static_cast<unsigned int>(Main_I2C_Result.errors[5]));
    Serial.printf("[Main I2C] first_error=0x%02X@0x%02X last_error=0x%02X@0x%02X\r\n",
                  static_cast<unsigned int>(Main_I2C_Result.first_error),
                  static_cast<unsigned int>(Main_I2C_Result.first_error_address),
                  static_cast<unsigned int>(Main_I2C_Result.last_error),
                  static_cast<unsigned int>(Main_I2C_Result.last_error_address));
    Release_Main_I2C();
    Serial.println("[Main I2C] bus_released=yes");
    return Main_I2C_Result.devices > 0;
}

void setup()
{
    Wait_For_Serial();
    Main_I2C_Test_Ready = Scan_Main_I2C();
    Serial.printf("[Result] Main I2C test=%s; LoRa initialized=no\r\n",
                  Main_I2C_Test_Ready ? "PASS" : "FAILED_OR_SKIPPED");
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    if (millis() >= Next_Heartbeat_Ms)
    {
        Serial.printf("[Heartbeat] Main I2C test=%s idle=%s devices=%u probes=%u\r\n",
                      Main_I2C_Test_Ready ? "PASS" : "FAIL",
                      Main_I2C_Result.idle ? "yes" : "no",
                      static_cast<unsigned int>(Main_I2C_Result.devices),
                      static_cast<unsigned int>(Main_I2C_Result.probes));
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    delay(10);
}
