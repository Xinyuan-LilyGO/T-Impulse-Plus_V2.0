#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <Wire.h>

#include <RadioLib.h>

#include "pin_config.h"

#define RADIO_FREQ 868.0

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t LORA_TRANSMIT_INTERVAL_MS = 5000UL;
static constexpr size_t LORA_PAYLOAD_BUFFER_SIZE = 96;

// P0.11 is SX1262 DIO1 and main-I2C SCL. This standalone test owns it for LoRa.
SPIClass Custom_SPI(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI);

static bool Main_I2C_Released = false;
static bool Radio_Initialized = false;
static bool Radio_Test_Ready = false;
static uint32_t Next_Transmit_Ms = 0;
static uint32_t Next_Failure_Report_Ms = 0;
static uint32_t Transmit_Sequence = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_lora_transmit] Serial ready");
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

static void Release_Main_I2C()
{
    Wire.end();
    pinMode(IIC_SDA_1, INPUT);
    pinMode(IIC_SCL_1, INPUT);
    nrf_gpio_cfg_default(IIC_SDA_1);
    nrf_gpio_cfg_default(IIC_SCL_1);
    Main_I2C_Released = true;
    Serial.println("[Resource] Wire ended; main-I2C SDA=P1.08/SCL=P0.11 released");
}

static void Release_Radio_Pins()
{
    pinMode(SX1262_MISO, INPUT);
    pinMode(SX1262_MOSI, INPUT);
    pinMode(SX1262_SCLK, INPUT);
    pinMode(SX1262_CS, INPUT);
    pinMode(SX1262_DIO1, INPUT);
    pinMode(SX1262_RST, INPUT);
    pinMode(SX1262_BUSY, INPUT);
    pinMode(SX1262_DIO2, INPUT);
    pinMode(SX1262_RF_VC1, INPUT);
    pinMode(SX1262_RF_VC2, INPUT);

    nrf_gpio_cfg_default(SX1262_MISO);
    nrf_gpio_cfg_default(SX1262_MOSI);
    nrf_gpio_cfg_default(SX1262_SCLK);
    nrf_gpio_cfg_default(SX1262_CS);
    nrf_gpio_cfg_default(SX1262_DIO1);
    nrf_gpio_cfg_default(SX1262_RST);
    nrf_gpio_cfg_default(SX1262_BUSY);
    nrf_gpio_cfg_default(SX1262_DIO2);
    nrf_gpio_cfg_default(SX1262_RF_VC1);
    nrf_gpio_cfg_default(SX1262_RF_VC2);
}

static void Shutdown_LoRa_Transmit()
{
    radio.clearDio1Action();

    if (Radio_Initialized)
    {
        const int16_t state = radio.sleep();
        if (state != RADIOLIB_ERR_NONE)
        {
            Serial.print("[SX1262] Sleep failed, RadioLib error code=");
            Serial.println(state);
        }
    }

    Custom_SPI.end();
    Release_Radio_Pins();
    Radio_Initialized = false;
    Radio_Test_Ready = false;
    Serial.println("[Resource] SX1262 callback, SPI, and radio pins released=yes");
}

static bool Check_Radio_State(const char *stage, int16_t state)
{
    if (state == RADIOLIB_ERR_NONE)
    {
        return true;
    }

    Serial.print("[SX1262] ");
    Serial.print(stage);
    Serial.print(" failed, RadioLib error code=");
    Serial.println(state);
    Shutdown_LoRa_Transmit();
    Next_Failure_Report_Ms = millis();
    return false;
}

static bool Initialize_LoRa()
{
    Serial.println("[Stage] V2 SX1262 LoRa transmit test starting");
    Serial.println("[Pin map] CS=P0.29 RST=P0.03 SCLK=P1.14 MOSI=P0.28 MISO=P0.30 BUSY=P1.12");
    Serial.println("[Pin map] DIO1=P0.11 DIO2=P0.31 RF_VC1=P1.13 RF_VC2=P1.10");
    Serial.println("[Safety] P0.11 is shared with main-I2C SCL; release Wire before starting LoRa");

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");

    Release_Main_I2C();
    pinMode(SX1262_CS, OUTPUT);
    digitalWrite(SX1262_CS, HIGH);
    pinMode(SX1262_DIO1, INPUT);
    pinMode(SX1262_BUSY, INPUT);
    pinMode(SX1262_DIO2, INPUT);

    Serial.println("[Stage] Main I2C released; starting NRF_SPIM3");
    Custom_SPI.begin();
    Serial.println("[SX1262] Parameters 868.0MHz/125kHz/SF10/CR4/6/sync=0xAB/22dBm/preamble=15/CRC=off/TCXO=3.0V/DC-DC");

    int16_t state = radio.begin(868.0, 125.0, 10, 6, 0xAB, 22, 15, 3.0, false);
    if (!Check_Radio_State("Initialization", state))
    {
        return false;
    }
    Radio_Initialized = true;

    radio.setRfSwitchPins(SX1262_RF_VC2, SX1262_RF_VC1);
    if (!Check_Radio_State("Set frequency", radio.setFrequency(RADIO_FREQ)))
    {
        return false;
    }
    if (!Check_Radio_State("Set bandwidth", radio.setBandwidth(125.0)))
    {
        return false;
    }
    if (!Check_Radio_State("Set spreading factor", radio.setSpreadingFactor(10)))
    {
        return false;
    }
    if (!Check_Radio_State("Set coding rate", radio.setCodingRate(6)))
    {
        return false;
    }
    if (!Check_Radio_State("Set sync word", radio.setSyncWord(0xAB)))
    {
        return false;
    }
    if (!Check_Radio_State("Set output power", radio.setOutputPower(22)))
    {
        return false;
    }
    if (!Check_Radio_State("Set current limit", radio.setCurrentLimit(140)))
    {
        return false;
    }
    if (!Check_Radio_State("Set preamble", radio.setPreambleLength(15)))
    {
        return false;
    }
    if (!Check_Radio_State("Disable CRC", radio.setCRC(false)))
    {
        return false;
    }

    Radio_Test_Ready = true;
    Next_Transmit_Ms = millis();
    Serial.println("[SX1262] Initialization successful; transmit loop is ready");
    return true;
}

static bool Transmit_Test_Packet()
{
    char payload[LORA_PAYLOAD_BUFFER_SIZE] = {};
    const uint32_t sequence = Transmit_Sequence++;
    snprintf(payload,
             sizeof(payload),
             "T-Impulse-Plus V2 TX seq=%lu uptime_ms=%lu",
             static_cast<unsigned long>(sequence),
             static_cast<unsigned long>(millis()));

    Serial.printf("[SX1262] Transmitting seq=%lu payload=\"%s\"\r\n",
                  static_cast<unsigned long>(sequence),
                  payload);
    const int16_t state = radio.transmit(payload);
    if (!Check_Radio_State("Transmit", state))
    {
        return false;
    }

    Serial.printf("[SX1262] Transmit successful seq=%lu\r\n",
                  static_cast<unsigned long>(sequence));
    return true;
}

void setup()
{
    Wait_For_Serial();
    Radio_Test_Ready = Initialize_LoRa();
    if (!Radio_Test_Ready)
    {
        Serial.println("[Result] SX1262 transmit test failed; radio resources released");
        Next_Failure_Report_Ms = millis();
    }
}

void loop()
{
    if (!Radio_Test_Ready)
    {
        if (millis() >= Next_Failure_Report_Ms)
        {
            Serial.println("[Heartbeat] SX1262 transmitter is not ready; check pins, power, TCXO, and RadioLib error code");
            Next_Failure_Report_Ms = millis() + 5000UL;
        }
        delay(10);
        return;
    }

    if (millis() >= Next_Transmit_Ms)
    {
        Transmit_Test_Packet();
        if (Radio_Test_Ready)
        {
            Next_Transmit_Ms = millis() + LORA_TRANSMIT_INTERVAL_MS;
        }
    }
    delay(1);
}
