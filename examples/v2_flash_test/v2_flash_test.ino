#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "Adafruit_SPIFlash.h"
#include "pin_config.h"
#include "zd25_flash_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 2000UL;

static constexpr uint32_t EXPECTED_JEDEC_ID = 0xBA4016UL;

Adafruit_FlashTransport_QSPI flashTransport(
    ZD25WQ32C_SCLK,
    ZD25WQ32C_CS,
    ZD25WQ32C_IO0,
    ZD25WQ32C_IO1,
    ZD25WQ32C_IO2,
    ZD25WQ32C_IO3);
Adafruit_SPIFlash flash(&flashTransport);

static bool Flash_Test_Ready = false;
static uint32_t Flash_Jedec_Id = 0;
static uint32_t Flash_Size_Bytes = 0;
static uint32_t Next_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_flash_test] Serial ready");
    Serial.flush();
}

static void Enable_3V3_Rail()
{
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    delay(POWER_SETTLE_TIME_MS);
    digitalWrite(RT9080_EN, LOW);
    delay(POWER_SETTLE_TIME_MS);
    digitalWrite(RT9080_EN, HIGH);
    delay(POWER_SETTLE_TIME_MS);
}

static void Release_Flash_Pins()
{
    pinMode(ZD25WQ32C_SCLK, INPUT);
    pinMode(ZD25WQ32C_CS, INPUT);
    pinMode(ZD25WQ32C_IO0, INPUT);
    pinMode(ZD25WQ32C_IO1, INPUT);
    pinMode(ZD25WQ32C_IO2, INPUT);
    pinMode(ZD25WQ32C_IO3, INPUT);
}

static bool Run_Flash_Test()
{
    Serial.println("[Stage] V2 QSPI flash diagnostic starting");
    Serial.println("[Pin map] Flash CS=P0.12 SCLK=P0.04 IO0=P0.06 IO1=P1.09 IO2=P0.08 IO3=P0.26");
    Serial.println("[Safety] Read JEDEC ID and capacity only; erase, write, and filesystem operations are disabled");

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");
    Serial.println("[Flash] stage=QSPI start; clock is configured by the Adafruit transport");

    if (!flash.begin(ZD25WQ32_DEVICES, ZD25WQ32_DEVICE_COUNT))
    {
        Serial.println("[Flash] initialization failed, failure_stage=flash.begin");
        flash.end();
        Release_Flash_Pins();
        Serial.println("[Flash] pins_released=yes");
        return false;
    }

    Flash_Jedec_Id = flash.getJEDECID();
    Flash_Size_Bytes = flash.size();
    const bool identity_match = Flash_Jedec_Id == EXPECTED_JEDEC_ID;
    Serial.printf("[Flash] initialized JEDEC=0x%06lX capacity=%lu bytes (%lu kbytes)\r\n",
                  static_cast<unsigned long>(Flash_Jedec_Id),
                  static_cast<unsigned long>(Flash_Size_Bytes),
                  static_cast<unsigned long>(Flash_Size_Bytes / 1024UL));
    Serial.printf("[Flash] identity_check expected=0x%06lX match=%s\r\n",
                  static_cast<unsigned long>(EXPECTED_JEDEC_ID),
                  identity_match ? "yes" : "no");

    flash.end();
    Release_Flash_Pins();
    Serial.println("[Flash] stage=complete pins_released=yes");
    return identity_match && (Flash_Size_Bytes != 0);
}

void setup()
{
    Wait_For_Serial();
    Flash_Test_Ready = Run_Flash_Test();
    Serial.printf("[Result] Flash read-only test=%s\r\n",
                  Flash_Test_Ready ? "PASS" : "FAIL");
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    if (millis() >= Next_Heartbeat_Ms)
    {
        Serial.printf("[Heartbeat] Flash ready=%s JEDEC=0x%06lX capacity_KB=%lu pins_released=yes\r\n",
                      Flash_Test_Ready ? "yes" : "no",
                      static_cast<unsigned long>(Flash_Jedec_Id),
                      static_cast<unsigned long>(Flash_Size_Bytes / 1024UL));
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    delay(10);
}
