#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t CONTROL_SETTLE_TIME_MS = 200UL;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 5000UL;

static uint32_t Gps_En_High_Transitions = 0;
static uint32_t Gps_En_Low_Transitions = 0;
static int Gps_En_Last_Level = HIGH;
static uint32_t Next_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_gnss_pps_test] Serial ready");
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

static void Print_Gps_Control_State(const char *phase)
{
    const int level = digitalRead(GPS_EN);
    Serial.printf("[GNSS] phase=%s P0.24/GPS_EN output_level=%d measure_GPS_VDD_with_multimeter\r\n",
                  phase,
                  level);
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] V2 GNSS shared-net diagnostic starting");
    Serial.println("[Pin map] P0.24 is labeled GPS_EN and legacy GPS_1PPS; both names refer to the same physical net");
    Serial.println("[Safety] Keep P0.24 as an output; do not attach a PPS interrupt or initialize Serial2, I2C, SPI, LoRa, or the display");

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");

    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, HIGH);
    Gps_En_High_Transitions++;
    delay(CONTROL_SETTLE_TIME_MS);
    Gps_En_Last_Level = digitalRead(GPS_EN);
    Print_Gps_Control_State("OFF");

    digitalWrite(GPS_EN, LOW);
    Gps_En_Low_Transitions++;
    delay(CONTROL_SETTLE_TIME_MS);
    Gps_En_Last_Level = digitalRead(GPS_EN);
    Print_Gps_Control_State("ON");
    Serial.println("[Result] Only the GPS_EN control level was checked; P0.24 cannot provide an independent PPS measurement");
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    const int level = digitalRead(GPS_EN);
    if (level != Gps_En_Last_Level)
    {
        Gps_En_Last_Level = level;
        Serial.printf("[GNSS] P0.24/GPS_EN external state change level=%d measure_GPS_VDD\r\n",
                      level);
    }

    if (millis() >= Next_Heartbeat_Ms)
    {
        Serial.printf("[Heartbeat] P0.24/GPS_EN=%d OFF_count=%lu ON_count=%lu measure_GPS_VDD\r\n",
                      level,
                      static_cast<unsigned long>(Gps_En_High_Transitions),
                      static_cast<unsigned long>(Gps_En_Low_Transitions));
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    delay(1);
}
