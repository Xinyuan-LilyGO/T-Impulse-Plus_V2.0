#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <Wire.h>

#include <RadioLib.h>

#include "pin_config.h"

#define RADIO_FREQ 868.0

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t LORA_IRQ_POLL_INTERVAL_MS = 20UL;
static constexpr uint32_t LORA_STATUS_REPORT_INTERVAL_MS = 5000UL;
static constexpr size_t LORA_PACKET_BUFFER_SIZE = 256;
static constexpr uint32_t LORA_RX_EVENT_IRQ_FLAGS =
    RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_CRC_ERR;

SPIClass Custom_SPI(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI);

volatile bool receivedFlag = false;
volatile uint32_t dio1EdgeCount = 0;
static bool Radio_Initialized = false;
static bool Radio_Test_Ready = false;
static uint32_t Last_Irq_Poll_Ms = 0;
static uint32_t Last_Status_Report_Ms = 0;
static uint32_t Last_Irq_Flags = 0;
static uint32_t Next_Failure_Report_Ms = 0;

static void set_receive_flag()
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
    Serial.println("[T-Impulse_Plus_v2.0][v2_lora_test] Serial ready");
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
            Serial.printf("[SX1262] Sleep failed, RadioLib error code=%d\r\n", state);
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
    Serial.printf("[SX1262] %s failed, RadioLib error code=%d\r\n", stage, state);
    Shutdown_LoRa_Test();
    Next_Failure_Report_Ms = millis();
    return false;
}

static bool Start_Receiving()
{
    if (!Check_Radio_State("Start receive", radio.startReceive()))
    {
        return false;
    }
    Last_Irq_Poll_Ms = millis();
    return true;
}

static bool Initialize_LoRa()
{
    Serial.println("[Stage] V2 SX1262 LoRa receive test starting");
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
    if (!Check_Radio_State("Set current limit", radio.setCurrentLimit(140)) ||
        !Check_Radio_State("Disable CRC", radio.setCRC(false)))
    {
        return false;
    }
    radio.setPacketReceivedAction(set_receive_flag);
    if (!Start_Receiving())
    {
        return false;
    }
    Last_Status_Report_Ms = millis();
    Last_Irq_Flags = radio.getIrqFlags();
    Radio_Test_Ready = true;
    Serial.println("[SX1262] Initialization successful; receive mode is ready");
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

static void Handle_Receive_Event(bool dio1_event)
{
    receivedFlag = false;
    const uint32_t event_flags = radio.getIrqFlags();
    Last_Irq_Flags = event_flags;
    const size_t packet_length = radio.getPacketLength();
    const size_t safe_length = packet_length > 0 && packet_length < LORA_PACKET_BUFFER_SIZE
                                   ? packet_length
                                   : LORA_PACKET_BUFFER_SIZE - 1;
    uint8_t packet[LORA_PACKET_BUFFER_SIZE] = {};
    Serial.printf("[SX1262] Receive event source=%s packet_length=%lu\r\n",
                  dio1_event ? "DIO1" : "SPI-poll",
                  static_cast<unsigned long>(packet_length));
    const int16_t state = radio.readData(packet, safe_length);
    if (state == RADIOLIB_ERR_NONE)
    {
        Print_Packet_Hex(packet, packet_length < safe_length ? packet_length : safe_length);
        Serial.printf("[SX1262] RSSI=%.2f dBm SNR=%.2f dB frequency_error=%.2f Hz\r\n",
                      radio.getRSSI(), radio.getSNR(), radio.getFrequencyError());
    }
    else if (state == RADIOLIB_ERR_CRC_MISMATCH ||
             (event_flags & RADIOLIB_SX126X_IRQ_CRC_ERR) != 0)
    {
        Serial.printf("[SX1262] CRC error, RadioLib error code=%d\r\n", state);
    }
    else
    {
        Serial.printf("[SX1262] Read receive data failed, RadioLib error code=%d\r\n", state);
    }
    if (Radio_Test_Ready && Start_Receiving())
    {
        Serial.println("[SX1262] Receive listener restored");
    }
}

void setup()
{
    Wait_For_Serial();
    Radio_Test_Ready = Initialize_LoRa();
    if (!Radio_Test_Ready)
    {
        Serial.println("[Result] SX1262 receive test failed; radio resources released");
        Next_Failure_Report_Ms = millis();
    }
}

void loop()
{
    if (!Radio_Test_Ready)
    {
        if (millis() >= Next_Failure_Report_Ms)
        {
            Serial.println("[Heartbeat] SX1262 receiver is not ready; check pins, power, TCXO, and RadioLib error code");
            Next_Failure_Report_Ms = millis() + 5000UL;
        }
        delay(10);
        return;
    }
    const uint32_t now = millis();
    const bool dio1_event = receivedFlag;
    bool receive_event = dio1_event;
    if (now - Last_Irq_Poll_Ms >= LORA_IRQ_POLL_INTERVAL_MS)
    {
        Last_Irq_Poll_Ms = now;
        Last_Irq_Flags = radio.getIrqFlags();
        receive_event = receive_event || ((Last_Irq_Flags & LORA_RX_EVENT_IRQ_FLAGS) != 0);
    }
    if (now - Last_Status_Report_Ms >= LORA_STATUS_REPORT_INTERVAL_MS)
    {
        Last_Status_Report_Ms = now;
        Serial.printf("[SX1262] Listening irq=0x%lX DIO1=%d BUSY=%d VC1=%d VC2=%d DIO1_edges=%lu\r\n",
                      static_cast<unsigned long>(Last_Irq_Flags),
                      digitalRead(SX1262_DIO1), digitalRead(SX1262_BUSY),
                      digitalRead(SX1262_RF_VC1), digitalRead(SX1262_RF_VC2),
                      static_cast<unsigned long>(dio1EdgeCount));
    }
    if (receive_event)
    {
        Handle_Receive_Event(dio1_event);
    }
    delay(1);
}
