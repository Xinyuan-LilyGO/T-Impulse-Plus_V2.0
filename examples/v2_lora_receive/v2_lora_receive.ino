#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <Wire.h>

#include <RadioLib.h>

#include "pin_config.h"

#define RADIO_FREQ  868.0

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t LORA_IRQ_POLL_INTERVAL_MS = 20UL;
static constexpr uint32_t LORA_STATUS_REPORT_INTERVAL_MS = 5000UL;
static constexpr size_t LORA_PACKET_BUFFER_SIZE = 256;
static constexpr uint32_t LORA_RX_EVENT_IRQ_FLAGS =
    RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_CRC_ERR;

// P0.11 is SX1262 DIO1 and main-I2C SCL. This standalone test owns it for LoRa.
SPIClass Custom_SPI(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI);

volatile bool receivedFlag = false;
volatile uint32_t dio1EdgeCount = 0;
static bool Main_I2C_Released = false;
static bool Radio_Initialized = false;
static bool Radio_Test_Ready = false;
static uint32_t lastIrqPollMs = 0;
static uint32_t lastStatusReportMs = 0;
static uint32_t lastIrqFlags = 0;
static uint32_t nextFailureReportMs = 0;

void set_receive_flag(void)
{
    receivedFlag = true;
    dio1EdgeCount++;
}

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_lora_receive] Serial ready");
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

static void Shutdown_LoRa_Test()
{
    radio.clearDio1Action();
    receivedFlag = false;

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

static void Fail_LoRa(const __FlashStringHelper *stage, int16_t state)
{
    Serial.print("[SX1262] ");
    Serial.print(stage);
    Serial.print(F(" failed, RadioLib error code="));
    Serial.println(state);
    Shutdown_LoRa_Test();
    nextFailureReportMs = millis();
}

static bool Check_Radio_State(const __FlashStringHelper *stage, int16_t state)
{
    if (state != RADIOLIB_ERR_NONE)
    {
        Fail_LoRa(stage, state);
        return false;
    }
    return true;
}

static bool Start_Receiving()
{
    const int16_t state = radio.startReceive();
    if (!Check_Radio_State(F("Start receive"), state))
    {
        return false;
    }
    lastIrqPollMs = millis();
    return true;
}

static void Print_Packet_Hex(const uint8_t *packet, size_t length)
{
    Serial.print(F("[SX1262] Raw packet="));
    for (size_t index = 0; index < length; index++)
    {
        if (packet[index] < 0x10)
        {
            Serial.print('0');
        }
        Serial.print(packet[index], HEX);
        if (index + 1 < length)
        {
            Serial.print(' ');
        }
    }
    Serial.println();
}

static bool Initialize_LoRa()
{
    Serial.println("[Stage] V2 SX1262 LoRa receive diagnostic starting");
    Serial.println("[Pin map] CS=P0.29 RST=P0.03 SCLK=P1.14 MOSI=P0.28 MISO=P0.30 BUSY=P1.12");
    Serial.println("[Pin map] DIO1=P0.11 DIO2=P0.31 RF_VC1=P1.13 RF_VC2=P1.10");
    Serial.println("[Safety] P0.11 is shared with main-I2C SCL; release Wire first; this example is receive-only");

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

    Serial.println(F("[SX1262] Parameters 868.0MHz/125kHz/SF10/CR4/6/sync=0xAB/22dBm/preamble=15/CRC=off/TCXO=3.0V/DC-DC"));
    int16_t state = radio.begin(868.0, 125.0, 10, 6, 0xAB, 22, 15, 3.0, false);
    if (!Check_Radio_State(F("Initialization"), state))
    {
        return false;
    }
    Radio_Initialized = true;

    radio.setRfSwitchPins(SX1262_RF_VC2, SX1262_RF_VC1);
    if (!Check_Radio_State(F("Set frequency"), radio.setFrequency(RADIO_FREQ)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set bandwidth"), radio.setBandwidth(125.0)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set spreading factor"), radio.setSpreadingFactor(10)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set coding rate"), radio.setCodingRate(6)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set sync word"), radio.setSyncWord(0xAB)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set output power"), radio.setOutputPower(22)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set current limit"), radio.setCurrentLimit(140)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Set preamble"), radio.setPreambleLength(15)))
    {
        return false;
    }
    if (!Check_Radio_State(F("Disable CRC"), radio.setCRC(false)))
    {
        return false;
    }

    radio.setPacketReceivedAction(set_receive_flag);
    if (!Start_Receiving())
    {
        return false;
    }

    lastStatusReportMs = millis();
    lastIrqFlags = radio.getIrqFlags();
    Radio_Test_Ready = true;
    Serial.println("[SX1262] Initialization successful; receive-only mode is active");
    return true;
}

static void Print_Status(uint32_t now)
{
    if (now - lastStatusReportMs < LORA_STATUS_REPORT_INTERVAL_MS)
    {
        return;
    }
    lastStatusReportMs = now;
    Serial.print(F("[SX1262] Listening irq=0x"));
    Serial.print(lastIrqFlags, HEX);
    Serial.print(F(" DIO1="));
    Serial.print(digitalRead(SX1262_DIO1));
    Serial.print(F(" BUSY="));
    Serial.print(digitalRead(SX1262_BUSY));
    Serial.print(F(" VC1="));
    Serial.print(digitalRead(SX1262_RF_VC1));
    Serial.print(F(" VC2="));
    Serial.print(digitalRead(SX1262_RF_VC2));
    Serial.print(F(" DIO1_edges="));
    Serial.println(dio1EdgeCount);
}

static void Handle_Receive_Event(bool dio1Event)
{
    receivedFlag = false;
    const uint32_t eventFlags = radio.getIrqFlags();
    lastIrqFlags = eventFlags;
    const size_t packetLength = radio.getPacketLength();
    const size_t safeLength = packetLength > 0 && packetLength < LORA_PACKET_BUFFER_SIZE
                                  ? packetLength
                                  : LORA_PACKET_BUFFER_SIZE - 1;
    uint8_t packet[LORA_PACKET_BUFFER_SIZE] = {};

    Serial.print(F("[SX1262] Receive event source="));
    Serial.println(dio1Event ? F("DIO1") : F("SPI-poll"));
    Serial.print(F("[SX1262] packetLength="));
    Serial.println(static_cast<unsigned long>(packetLength));

    const int16_t state = radio.readData(packet, safeLength);
    if (state == RADIOLIB_ERR_NONE)
    {
        Print_Packet_Hex(packet, packetLength < safeLength ? packetLength : safeLength);
        Serial.print(F("[SX1262] RSSI="));
        Serial.print(radio.getRSSI());
        Serial.print(F(" dBm SNR="));
        Serial.print(radio.getSNR());
        Serial.print(F(" dB frequency_error="));
        Serial.print(radio.getFrequencyError());
        Serial.println(F(" Hz"));
    }
    else if (state == RADIOLIB_ERR_CRC_MISMATCH ||
             (eventFlags & RADIOLIB_SX126X_IRQ_CRC_ERR) != 0)
    {
        Serial.print(F("[SX1262] CRC error, RadioLib error code="));
        Serial.println(state);
    }
    else
    {
        Serial.print(F("[SX1262] Read receive data failed, RadioLib error code="));
        Serial.println(state);
    }

    if (Radio_Test_Ready)
    {
        Serial.println(F("[SX1262] Receive listener restored"));
        Start_Receiving();
    }
}

void setup()
{
    Wait_For_Serial();
    Radio_Test_Ready = Initialize_LoRa();
    if (!Radio_Test_Ready)
    {
        Serial.println(F("[Result] SX1262 receive test failed; radio resources released"));
        nextFailureReportMs = millis();
    }
}

void loop()
{
    if (!Radio_Test_Ready)
    {
        if (millis() >= nextFailureReportMs)
        {
            Serial.println(F("[Heartbeat] SX1262 is not ready; check pins, power, TCXO, and the RadioLib error code"));
            nextFailureReportMs = millis() + 5000UL;
        }
        delay(10);
        return;
    }

    const uint32_t now = millis();
    const bool dio1Event = receivedFlag;
    bool receiveEvent = dio1Event;
    if (now - lastIrqPollMs >= LORA_IRQ_POLL_INTERVAL_MS)
    {
        lastIrqPollMs = now;
        lastIrqFlags = radio.getIrqFlags();
        receiveEvent = receiveEvent || ((lastIrqFlags & LORA_RX_EVENT_IRQ_FLAGS) != 0);
    }

    Print_Status(now);
    if (receiveEvent)
    {
        Handle_Receive_Event(dio1Event);
    }
    delay(1);
}
