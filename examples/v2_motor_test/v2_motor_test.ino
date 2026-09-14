#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 3000UL;
static constexpr uint32_t MAX_MOTOR_PULSE_MS = 150UL;
static constexpr uint16_t MOTOR_PULSE_MS[] = {50, 100, 150};
static constexpr uint8_t MOTOR_PULSE_COUNT =
    sizeof(MOTOR_PULSE_MS) / sizeof(MOTOR_PULSE_MS[0]);

static uint8_t Motor_Pulse_Index = 0;
static bool Motor_Test_Done = false;
static uint32_t Next_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_motor_test] Serial ready");
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

static void Run_Motor_Pulse(uint16_t duration_ms)
{
    Serial.printf("[Motor] pulse_start index=%u duration=%ums P0.22_before=%d\r\n",
                  static_cast<unsigned int>(Motor_Pulse_Index + 1),
                  static_cast<unsigned int>(duration_ms),
                  digitalRead(VIBRATION_MOTOR_DATA));
    digitalWrite(VIBRATION_MOTOR_DATA, HIGH);
    const uint32_t started_ms = millis();
    delay(duration_ms);
    const int high_level = digitalRead(VIBRATION_MOTOR_DATA);
    digitalWrite(VIBRATION_MOTOR_DATA, LOW);
    Serial.printf("[Motor] pulse_end actual_duration=%lums high_readback=%d end_level=%d\r\n",
                  static_cast<unsigned long>(millis() - started_ms),
                  high_level,
                  digitalRead(VIBRATION_MOTOR_DATA));
    Motor_Pulse_Index++;
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] V2 motor diagnostic starting");
    Serial.println("[Pin map] VIBRATION_MOTOR_DATA=P0.22");
    Serial.printf("[Safety] Only three 50/100/150ms pulses are generated; maximum pulse=%lu ms\r\n",
                  static_cast<unsigned long>(MAX_MOTOR_PULSE_MS));

    Enable_3V3_Rail();
    pinMode(VIBRATION_MOTOR_DATA, OUTPUT);
    digitalWrite(VIBRATION_MOTOR_DATA, LOW);
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence; motor P0.22 starts LOW");

    for (uint8_t index = 0; index < MOTOR_PULSE_COUNT; index++)
    {
        Run_Motor_Pulse(MOTOR_PULSE_MS[index]);
        delay(1000);
    }
    digitalWrite(VIBRATION_MOTOR_DATA, LOW);
    Motor_Test_Done = true;
    Serial.println("[Motor] Three pulses complete; P0.22 remains LOW");
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    if (millis() >= Next_Heartbeat_Ms)
    {
        Serial.printf("[Heartbeat] Motor test=%s P0.22=%d pulse_count=%u\r\n",
                      Motor_Test_Done ? "complete" : "running",
                      digitalRead(VIBRATION_MOTOR_DATA),
                      static_cast<unsigned int>(Motor_Pulse_Index));
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    delay(10);
}
