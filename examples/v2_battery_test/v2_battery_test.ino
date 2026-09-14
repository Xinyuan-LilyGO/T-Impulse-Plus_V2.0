#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t SAMPLE_SETTLE_MS = 100UL;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 3000UL;
static constexpr uint8_t SAMPLE_COUNT = 16;
static constexpr float ADC_REFERENCE_MV = 3000.0F;
static constexpr float ADC_COUNTS = 4096.0F;
static constexpr float BATTERY_DIVIDER_RATIO = 2.0F;

struct Adc_Sample_Stats
{
    uint16_t minimum = 0xFFFF;
    uint16_t maximum = 0;
    uint32_t total = 0;
};

static bool Battery_Control_State = false;
static uint32_t Sample_Cycle = 0;
static uint32_t Next_Sample_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_battery_test] Serial ready");
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

static Adc_Sample_Stats Read_Adc_Stats()
{
    Adc_Sample_Stats stats;
    for (uint8_t index = 0; index < SAMPLE_COUNT; index++)
    {
        const uint16_t value = analogRead(BATTERY_ADC_DATA);
        if (value < stats.minimum)
        {
            stats.minimum = value;
        }
        if (value > stats.maximum)
        {
            stats.maximum = value;
        }
        stats.total += value;
        delay(2);
    }
    return stats;
}

static void Print_Adc_Sample()
{
    digitalWrite(BATTERY_MEASUREMENT_CONTROL,
                 Battery_Control_State ? HIGH : LOW);
    delay(SAMPLE_SETTLE_MS);

    const Adc_Sample_Stats stats = Read_Adc_Stats();
    const float average = static_cast<float>(stats.total) /
                          static_cast<float>(SAMPLE_COUNT);
    const float adc_voltage = average * (ADC_REFERENCE_MV / ADC_COUNTS) /
                              1000.0F;
    const float battery_voltage = adc_voltage * BATTERY_DIVIDER_RATIO;

    Serial.printf("[Battery] cycle=%lu control=P0.17 state=%s readback=%d\r\n",
                  static_cast<unsigned long>(Sample_Cycle++),
                  Battery_Control_State ? "HIGH" : "LOW",
                  digitalRead(BATTERY_MEASUREMENT_CONTROL));
    Serial.printf("[Battery] ADC samples=%u min=%u max=%u average=%.02f\r\n",
                  static_cast<unsigned int>(SAMPLE_COUNT),
                  static_cast<unsigned int>(stats.minimum),
                  static_cast<unsigned int>(stats.maximum),
                  average);
    Serial.printf("[Battery] ADC voltage=%.03fV battery voltage=%.03fV scale=2.0\r\n",
                  adc_voltage,
                  battery_voltage);
    Serial.println("[Battery] Measure the battery terminal voltage with a multimeter; software readings are not a substitute");

    Battery_Control_State = !Battery_Control_State;
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] V2 battery ADC diagnostic starting");
    Serial.println("[Pin map] BATTERY_MEASUREMENT_CONTROL=P0.17 BATTERY_ADC_DATA=P0.05");
    Serial.println("[Safety] Only RT9080 and the battery ADC are initialized; I2C, SPI, display, GNSS, LoRa, BLE, and TTP223 stay disabled");

    Enable_3V3_Rail();
    pinMode(BATTERY_ADC_DATA, INPUT);
    pinMode(BATTERY_MEASUREMENT_CONTROL, OUTPUT);
    digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW);
    analogReference(AR_INTERNAL_3_0);
    analogReadResolution(12);

    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");
    Serial.println("[ADC] internal reference=3.0V resolution=12-bit sampling=16 samples for each LOW/HIGH state");
    Serial.println("[Stage] V2 battery ADC diagnostic ready");
    Next_Sample_Ms = millis();
}

void loop()
{
    if (millis() >= Next_Sample_Ms)
    {
        Print_Adc_Sample();
        Next_Sample_Ms = millis() + SAMPLE_INTERVAL_MS;
    }
    delay(10);
}
