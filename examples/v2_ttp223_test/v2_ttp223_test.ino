#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 20UL;
static constexpr uint32_t STABLE_TIME_MS = 60UL;
static constexpr uint32_t BASELINE_TIME_MS = 1000UL;
static constexpr int PRESSED_LEVEL = LOW;
static constexpr int RELEASED_LEVEL = HIGH;

static int Idle_Level = RELEASED_LEVEL;
static int Stable_Level = RELEASED_LEVEL;
static int Candidate_Level = RELEASED_LEVEL;
static uint32_t Candidate_Started_Ms = 0;
static uint32_t Touch_Count = 0;
static uint32_t Release_Count = 0;
static uint32_t Next_Sample_Ms = 0;
static uint32_t Next_Heartbeat_Ms = 0;
static bool Baseline_Ready = false;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_ttp223_test] Serial ready");
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

static void Establish_Baseline()
{
    uint32_t high_count = 0;
    uint32_t low_count = 0;
    const uint32_t started_ms = millis();
    while (millis() - started_ms < BASELINE_TIME_MS)
    {
        if (digitalRead(TTP223_KEY) == HIGH)
        {
            high_count++;
        }
        else
        {
            low_count++;
        }
        delay(SAMPLE_INTERVAL_MS);
    }

    Idle_Level = high_count >= low_count ? RELEASED_LEVEL : PRESSED_LEVEL;
    Stable_Level = Idle_Level;
    Candidate_Level = Idle_Level;
    Candidate_Started_Ms = millis();
    Baseline_Ready = true;
    Serial.printf("[Touch] baseline complete high_samples=%lu low_samples=%lu idle_level=%d\r\n",
                  static_cast<unsigned long>(high_count),
                  static_cast<unsigned long>(low_count),
                  Idle_Level);
    if (Idle_Level != RELEASED_LEVEL)
    {
        Serial.println("[Touch] Warning: Q=P0.15 is LOW at startup; check for a held touch or short circuit");
    }
    Serial.println("[Touch] TTP223 pressed=LOW released=HIGH; perform short and long touches");
}

static void Sample_Touch()
{
    const uint32_t now = millis();
    const int raw_level = digitalRead(TTP223_KEY);
    if (raw_level != Candidate_Level)
    {
        Candidate_Level = raw_level;
        Candidate_Started_Ms = now;
        return;
    }

    if (Candidate_Level == Stable_Level ||
        now - Candidate_Started_Ms < STABLE_TIME_MS)
    {
        return;
    }

    Stable_Level = Candidate_Level;
    if (Stable_Level == PRESSED_LEVEL)
    {
        Touch_Count++;
        Serial.printf("[Touch] event=pressed time=%lums level=LOW press_count=%lu\r\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned long>(Touch_Count));
    }
    else if (Stable_Level == RELEASED_LEVEL)
    {
        Release_Count++;
        Serial.printf("[Touch] event=released time=%lums level=HIGH release_count=%lu\r\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned long>(Release_Count));
    }
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] V2 TTP223 diagnostic starting");
    Serial.println("[Pin map] TTP223 Q=P0.15");
    Serial.println("[Safety] Only P0.15 is initialized as INPUT; display, I2C, SPI, GNSS, LoRa, and BLE stay disabled");

    Enable_3V3_Rail();
    pinMode(TTP223_KEY, INPUT);
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence; TTP223 input_mode=INPUT");
    Establish_Baseline();
    Next_Sample_Ms = millis();
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    if (Baseline_Ready && millis() >= Next_Sample_Ms)
    {
        Sample_Touch();
        Next_Sample_Ms = millis() + SAMPLE_INTERVAL_MS;
    }
    if (Baseline_Ready && millis() >= Next_Heartbeat_Ms)
    {
        Serial.printf("[Heartbeat] TTP223 idle=%d stable_level=%d raw_level=%d pressed=%lu released=%lu\r\n",
                      Idle_Level,
                      Stable_Level,
                      digitalRead(TTP223_KEY),
                      static_cast<unsigned long>(Touch_Count),
                      static_cast<unsigned long>(Release_Count));
        Next_Heartbeat_Ms = millis() + 2000UL;
    }
    delay(1);
}
