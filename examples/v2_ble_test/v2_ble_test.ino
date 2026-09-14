#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <string.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t POWER_SETTLE_TIME_MS = 100UL;
static constexpr uint32_t BLE_HEARTBEAT_INTERVAL_MS = 1000UL;

BLEDfu bledfu;
BLEDis bledis;
BLEUart bleuart;

static bool Ble_Connected = false;
static char Ble_Peer_Name[32] = {};
static uint32_t Next_Ble_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_ble_test] Serial ready");
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

static void Start_Advertising()
{
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleuart);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

static void On_Ble_Connect(uint16_t connection_handle)
{
    BLEConnection *connection = Bluefruit.Connection(connection_handle);
    connection->getPeerName(Ble_Peer_Name, sizeof(Ble_Peer_Name));
    Ble_Connected = true;
    Serial.printf("[BLE] Connected peer=%s handle=%u\r\n",
                  Ble_Peer_Name,
                  static_cast<unsigned int>(connection_handle));
}

static void On_Ble_Disconnect(uint16_t connection_handle, uint8_t reason)
{
    (void)connection_handle;
    Ble_Connected = false;
    Ble_Peer_Name[0] = '\0';
    Serial.printf("[BLE] Disconnected reason=0x%02X\r\n",
                  static_cast<unsigned int>(reason));
}

static void Send_Ble_Heartbeat()
{
    char message[128] = {};
    const int length = snprintf(
        message, sizeof(message),
        "V2 BLE test\r\nMAC:%s\r\nuptime:%lus\r\n",
        getMcuUniqueID(),
        static_cast<unsigned long>(millis() / 1000UL));
    if (length > 0)
    {
        bleuart.write(reinterpret_cast<const uint8_t *>(message),
                      static_cast<size_t>(length));
    }
}

static void Forward_Serial_To_Ble()
{
    while (Serial.available() && Ble_Connected)
    {
        uint8_t buffer[64] = {};
        const size_t count = min(static_cast<size_t>(Serial.available()),
                                 sizeof(buffer));
        Serial.readBytes(reinterpret_cast<char *>(buffer), count);
        bleuart.write(buffer, count);
    }
}

static void Forward_Ble_To_Serial()
{
    while (bleuart.available())
    {
        uint8_t buffer[64] = {};
        const size_t count = min(static_cast<size_t>(bleuart.available()),
                                 sizeof(buffer));
        bleuart.read(buffer, count);
        Serial.print("[BLE RX] ");
        Serial.write(buffer, count);
    }
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] BLE UART diagnostic starting");
    Serial.println("[Pin map] BLE uses the nRF52840 internal radio; no external GPIO is changed");
    Serial.println("[Safety] Only BLE and RT9080_EN are initialized; I2C, SPI, GNSS, LoRa, display, and ADC stay disabled");

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");
    Bluefruit.autoConnLed(true);
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    const bool ble_started = Bluefruit.begin();
    if (!ble_started)
    {
        Serial.println("[BLE] Bluefruit initialization failed; services and advertising stopped");
        return;
    }
    Bluefruit.setTxPower(8);
    Bluefruit.setName("T-Impulse-Plus-V2");
    Bluefruit.Periph.setConnectCallback(On_Ble_Connect);
    Bluefruit.Periph.setDisconnectCallback(On_Ble_Disconnect);

    bledfu.begin();
    bledis.setManufacturer("LILYGO Industries");
    bledis.setModel("T-Impulse-Plus V2");
    bledis.begin();
    bleuart.begin();
    Start_Advertising();
    Serial.println("[BLE] Advertising started; connect to the UART service with a BLE tool");
    Next_Ble_Heartbeat_Ms = millis() + BLE_HEARTBEAT_INTERVAL_MS;
}

void loop()
{
    if (Ble_Connected && millis() >= Next_Ble_Heartbeat_Ms)
    {
        Send_Ble_Heartbeat();
        Serial.printf("[Heartbeat] BLE connected peer=%s\r\n", Ble_Peer_Name);
        Next_Ble_Heartbeat_Ms = millis() + BLE_HEARTBEAT_INTERVAL_MS;
    }
    Forward_Serial_To_Ble();
    Forward_Ble_To_Serial();
    delay(5);
}
