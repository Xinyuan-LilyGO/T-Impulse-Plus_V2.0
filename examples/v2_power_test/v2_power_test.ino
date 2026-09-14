#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 1000UL;

static bool Power_Test_Ready = false;
static uint32_t Next_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_power_test] Serial ready");
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

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] V2 RT9080 rail test starting");
    Serial.println("[Pin map] RT9080_EN=P0.19");
    Serial.println("[Safety] Only the rail enable GPIO is tested; no ADC, bus, UART, BLE, motor, touch, display, or radio is initialized");

    Enable_3V3_Rail();
    Power_Test_Ready = digitalRead(RT9080_EN) == HIGH;
    Serial.printf("[Power] RT9080_EN=P0.19 readback=%d after high-low-high sequence\r\n",
                  digitalRead(RT9080_EN));
    Serial.println("[Power] Measure VDD3V3 with a multimeter; software cannot validate an external rail voltage");
    Serial.printf("[Result] RT9080 rail test=%s\r\n",
                  Power_Test_Ready ? "PASS" : "FAIL");
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    if (millis() >= Next_Heartbeat_Ms)
    {
        Serial.printf("[Heartbeat] RT9080_EN=%d rail_test=%s\r\n",
                      digitalRead(RT9080_EN),
                      Power_Test_Ready ? "ready" : "failed");
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    delay(10);
}
