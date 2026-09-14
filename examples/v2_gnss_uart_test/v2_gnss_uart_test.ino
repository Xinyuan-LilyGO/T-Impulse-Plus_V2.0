#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "TinyGPSPlus.h"
#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t GNSS_BAUD = 38400UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t GNSS_STATUS_INTERVAL_MS = 5000UL;
static constexpr size_t GNSS_RAW_LINE_BUFFER_SIZE = 128;

TinyGPSPlus gps;

static char Gnss_Raw_Line[GNSS_RAW_LINE_BUFFER_SIZE] = {};
static size_t Gnss_Raw_Line_Count = 0;
static uint32_t Gnss_Raw_Bytes = 0;
static uint32_t Gnss_Sentence_Count = 0;
static uint32_t Gnss_Line_Overflow_Count = 0;
static uint32_t Next_Gnss_Status_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_gnss_uart_test] Serial ready");
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

static void displayInfo()
{
    Serial.print("[GNSS] fix=");
    if (gps.location.isValid())
    {
        Serial.printf("valid latitude=%.06f longitude=%.06f",
                      gps.location.lat(),
                      gps.location.lng());
    }
    else
    {
        Serial.print("invalid");
    }

    Serial.print(" date_time=");
    if (gps.date.isValid() && gps.time.isValid())
    {
        Serial.printf("%04d-%02d-%02d %02d:%02d:%02d.%02d",
                      gps.date.year(),
                      gps.date.month(),
                      gps.date.day(),
                      gps.time.hour(),
                      gps.time.minute(),
                      gps.time.second(),
                      gps.time.centisecond());
    }
    else
    {
        Serial.print("invalid");
    }
    Serial.printf(" bytes=%lu sentences=%lu\r\n",
                  static_cast<unsigned long>(Gnss_Raw_Bytes),
                  static_cast<unsigned long>(Gnss_Sentence_Count));
}

static void Process_Gnss_Byte(uint8_t value)
{
    Gnss_Raw_Bytes++;
    if (Gnss_Raw_Line_Count < GNSS_RAW_LINE_BUFFER_SIZE - 1)
    {
        Gnss_Raw_Line[Gnss_Raw_Line_Count++] = static_cast<char>(value);
    }
    else if (value != '\n')
    {
        Gnss_Line_Overflow_Count++;
    }

    if (gps.encode(value))
    {
        Gnss_Sentence_Count++;
        displayInfo();
    }

    if (value == '\n')
    {
        Gnss_Raw_Line[Gnss_Raw_Line_Count] = '\0';
        Serial.printf("[GNSS][raw] %s", Gnss_Raw_Line);
        Gnss_Raw_Line_Count = 0;
        Gnss_Raw_Line[0] = '\0';
    }
}

static void Configure_Gnss_Uart()
{
    Serial.println("[Stage] GNSS UART initialization starting");
    Serial.println("[Pin map] GNSS module TX=P0.02 -> MCU RX; module RX=P1.15 <- MCU TX");
    Serial.println("[Safety] P0.24 is only the GPS_EN control net; measure GPS_VDD externally");

    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, HIGH);
    delay(POWER_SETTLE_TIME_MS);
    Serial.println("[GNSS] GPS_EN=HIGH; configuring Serial2");

    Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
    Serial2.begin(38400);
    Serial.printf("[GNSS] UART started baud=%lu TX=P0.02 RX=P1.15\r\n",
                  static_cast<unsigned long>(GNSS_BAUD));

    digitalWrite(GPS_EN, LOW);
    delay(POWER_SETTLE_TIME_MS);
    Serial.println("[GNSS] GPS_EN=LOW; module is in receive phase; verify GPS_VDD with a multimeter");
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] V2 GNSS UART diagnostic starting");
    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");
    Configure_Gnss_Uart();
    Next_Gnss_Status_Ms = millis() + GNSS_STATUS_INTERVAL_MS;
}

void loop()
{
    while (Serial2.available() > 0)
    {
        Process_Gnss_Byte(static_cast<uint8_t>(Serial2.read()));
    }

    const uint32_t now = millis();
    if (now >= Next_Gnss_Status_Ms)
    {
        Serial.printf("[GNSS] status raw_bytes=%lu sentences=%lu line_overflow=%lu GPS_EN=%d measure_GPS_VDD\r\n",
                      static_cast<unsigned long>(Gnss_Raw_Bytes),
                      static_cast<unsigned long>(Gnss_Sentence_Count),
                      static_cast<unsigned long>(Gnss_Line_Overflow_Count),
                      digitalRead(GPS_EN));
        if (Gnss_Raw_Bytes == 0)
        {
            Serial.println("[GNSS] No bytes received; check GPS_VDD, ground, TX/RX direction, and the 38400 baud rate");
        }
        Next_Gnss_Status_Ms = now + GNSS_STATUS_INTERVAL_MS;
    }
    delay(1);
}
