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
static constexpr uint32_t LORA_TRANSMIT_TIMEOUT_MS = 15000UL;
static constexpr uint32_t LORA_TRANSMIT_INTERVAL_MS = 5000UL;
static constexpr uint32_t TOUCH_SAMPLE_INTERVAL_MS = 20UL;
static constexpr uint32_t TOUCH_STABLE_TIME_MS = 60UL;
static constexpr uint32_t TOUCH_BASELINE_TIME_MS = 1000UL;
static constexpr int TOUCH_PRESSED_LEVEL = LOW;
static constexpr int TOUCH_RELEASED_LEVEL = HIGH;
static constexpr size_t LORA_PAYLOAD_BUFFER_SIZE = 96;

// P0.11 is both SX1262 DIO1 and the main-I2C SCL net.
SPIClass Custom_SPI(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI);

static bool Radio_Initialized = false;
static bool Radio_Test_Ready = false;
static uint32_t Next_Transmit_Ms = 0;
static uint32_t Next_Failure_Report_Ms = 0;
static uint32_t Transmit_Sequence = 0;
static uint32_t Candidate_Started_Ms = 0;
static uint32_t Next_Touch_Sample_Ms = 0;
static int Idle_Level = TOUCH_RELEASED_LEVEL;
static int Stable_Level = TOUCH_RELEASED_LEVEL;
static int Candidate_Level = TOUCH_RELEASED_LEVEL;
static bool External_LoRa_Antenna_Selected = false;
static bool Touch_Baseline_Ready = false;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_lora_external_antenna_transmit] Serial ready");
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

static const char *Selected_LoRa_Antenna_Name()
{
    return External_LoRa_Antenna_Selected ? "external" : "internal";
}

static void Apply_LoRa_Antenna_Selection()
{
    pinMode(LORA_ANTENNA_SPDT_VCTL, OUTPUT);
    if (External_LoRa_Antenna_Selected)
    {
        digitalWrite(LORA_ANTENNA_SPDT_VCTL, LOW);
        Serial.println("[Antenna] SPDT VCTL=P1.07 level=LOW; external LoRa antenna selected");
    }
    else
    {
        digitalWrite(LORA_ANTENNA_SPDT_VCTL, HIGH);
        Serial.println("[Antenna] SPDT VCTL=P1.07 level=HIGH; internal LoRa antenna selected");
    }
}

static void Toggle_LoRa_Antenna()
{
    External_LoRa_Antenna_Selected = !External_LoRa_Antenna_Selected;
    Apply_LoRa_Antenna_Selection();
}

static void Establish_Touch_Baseline()
{
    uint32_t high_count = 0;
    uint32_t low_count = 0;
    const uint32_t started_ms = millis();
    while (millis() - started_ms < TOUCH_BASELINE_TIME_MS)
    {
        if (digitalRead(TTP223_KEY) == TOUCH_RELEASED_LEVEL)
        {
            high_count++;
        }
        else
        {
            low_count++;
        }
        delay(TOUCH_SAMPLE_INTERVAL_MS);
    }

    Idle_Level = high_count >= low_count ? TOUCH_RELEASED_LEVEL : TOUCH_PRESSED_LEVEL;
    Stable_Level = Idle_Level;
    Candidate_Level = Idle_Level;
    Candidate_Started_Ms = millis();
    Touch_Baseline_Ready = true;
    Serial.printf("[Touch] baseline complete high_samples=%lu low_samples=%lu idle_level=%d\r\n",
                  static_cast<unsigned long>(high_count),
                  static_cast<unsigned long>(low_count),
                  Idle_Level);
    if (Idle_Level != TOUCH_RELEASED_LEVEL)
    {
        Serial.println("[Touch] Warning: TTP223 Q=P0.15 is LOW at startup; check for a held touch or short circuit");
    }
    Serial.println("[Touch] TTP223 pressed=LOW released=HIGH; each stable press toggles the LoRa antenna");
}

static void Initialize_Touch_Antenna_Control()
{
    pinMode(TTP223_KEY, INPUT);
    Apply_LoRa_Antenna_Selection();
    Establish_Touch_Baseline();
    Next_Touch_Sample_Ms = millis();
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
        now - Candidate_Started_Ms < TOUCH_STABLE_TIME_MS)
    {
        return;
    }

    Stable_Level = Candidate_Level;
    if (Stable_Level == TOUCH_PRESSED_LEVEL)
    {
        Toggle_LoRa_Antenna();
        Serial.printf("[Touch] event=pressed time=%lums antenna=%s\r\n",
                      static_cast<unsigned long>(now),
                      Selected_LoRa_Antenna_Name());
    }
    else if (Stable_Level == TOUCH_RELEASED_LEVEL)
    {
        Serial.printf("[Touch] event=released time=%lums antenna=%s\r\n",
                      static_cast<unsigned long>(now),
                      Selected_LoRa_Antenna_Name());
    }
}

static void Poll_Touch()
{
    if (!Touch_Baseline_Ready || millis() < Next_Touch_Sample_Ms)
    {
        return;
    }

    Sample_Touch();
    Next_Touch_Sample_Ms = millis() + TOUCH_SAMPLE_INTERVAL_MS;
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

    Apply_LoRa_Antenna_Selection();
    Radio_Initialized = false;
    Radio_Test_Ready = false;
    Serial.printf("[Resource] SX1262 callback, SPI, and radio pins released; SPDT remains %s\r\n",
                  Selected_LoRa_Antenna_Name());
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
    Serial.println("[Stage] V2 SX1262 LoRa antenna-select transmit test starting");
    Serial.println("[Pin map] CS=P0.29 RST=P0.03 SCLK=P1.14 MOSI=P0.28 MISO=P0.30 BUSY=P1.12");
    Serial.println("[Pin map] DIO1=P0.11 DIO2=P0.31 RF_VC1=P1.13 RF_VC2=P1.10");
    Serial.println("[Pin map] SPDT VCTL=P1.07; HIGH=internal LoRa antenna, LOW=external LoRa antenna");
    Serial.println("[Pin map] TTP223 Q=P0.15; stable press toggles the SPDT antenna path");
    Serial.println("[Safety] P0.11 is shared with main-I2C SCL; release Wire before starting LoRa");

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");

    Initialize_Touch_Antenna_Control();
    Release_Main_I2C();
    pinMode(SX1262_CS, OUTPUT);
    digitalWrite(SX1262_CS, HIGH);
    pinMode(SX1262_DIO1, INPUT);
    pinMode(SX1262_BUSY, INPUT);
    pinMode(SX1262_DIO2, INPUT);

    Serial.println("[Stage] Main I2C released; starting NRF_SPIM3");
    Custom_SPI.begin();
    Custom_SPI.setClockDivider(SPI_CLOCK_DIV2);
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
    Serial.printf("[SX1262] Initialization successful; %s-antenna transmit loop is ready\r\n",
                  Selected_LoRa_Antenna_Name());
    return true;
}

static bool Transmit_Test_Packet()
{
    char payload[LORA_PAYLOAD_BUFFER_SIZE] = {};
    const uint32_t sequence = Transmit_Sequence++;
    snprintf(payload,
             sizeof(payload),
             "T-Impulse-Plus V2 %s antenna TX seq=%lu uptime_ms=%lu",
             Selected_LoRa_Antenna_Name(),
             static_cast<unsigned long>(sequence),
             static_cast<unsigned long>(millis()));

    Serial.printf("[SX1262] Transmitting via %s antenna seq=%lu payload=\"%s\"\r\n",
                  Selected_LoRa_Antenna_Name(),
                  static_cast<unsigned long>(sequence),
                  payload);

    // DIO1 is shared with the board's main-I2C SCL net. Poll TX_DONE over SPI.
    const int16_t startState = radio.startTransmit(payload);
    if (!Check_Radio_State("Start transmit", startState))
    {
        return false;
    }

    const uint32_t startedMs = millis();
    uint32_t irqFlags = 0;
    bool transmitDone = false;
    while (millis() - startedMs < LORA_TRANSMIT_TIMEOUT_MS)
    {
        irqFlags = radio.getIrqFlags();
        if ((irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) != 0)
        {
            transmitDone = true;
            break;
        }
        delay(LORA_IRQ_POLL_INTERVAL_MS);
    }

    if (!transmitDone)
    {
        Serial.printf("[SX1262] Transmit timeout after %lu ms, irq=0x%08lX\r\n",
                      static_cast<unsigned long>(millis() - startedMs),
                      static_cast<unsigned long>(irqFlags));
        const int16_t cleanupState = radio.finishTransmit();
        if (cleanupState != RADIOLIB_ERR_NONE)
        {
            Serial.printf("[SX1262] Transmit cleanup failed, RadioLib error code=%d\r\n",
                          cleanupState);
        }
        Shutdown_LoRa_Transmit();
        Next_Failure_Report_Ms = millis();
        return false;
    }

    const int16_t finishState = radio.finishTransmit();
    if (!Check_Radio_State("Finish transmit", finishState))
    {
        return false;
    }

    Serial.printf("[SX1262] %s-antenna transmit successful seq=%lu irq=0x%08lX\r\n",
                  Selected_LoRa_Antenna_Name(),
                  static_cast<unsigned long>(sequence),
                  static_cast<unsigned long>(irqFlags));
    return true;
}

void setup()
{
    Wait_For_Serial();
    Radio_Test_Ready = Initialize_LoRa();
    if (!Radio_Test_Ready)
    {
        Serial.println("[Result] SX1262 antenna-select transmit test failed; radio resources released");
        Next_Failure_Report_Ms = millis();
    }
}

void loop()
{
    Poll_Touch();

    if (!Radio_Test_Ready)
    {
        if (millis() >= Next_Failure_Report_Ms)
        {
            Serial.println("[Heartbeat] Antenna-select transmitter is not ready; check SPDT P1.07, pins, power, TCXO, and RadioLib error code");
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
