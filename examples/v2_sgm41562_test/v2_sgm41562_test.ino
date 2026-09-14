#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>

#include "cpp_bus_driver_library.h"
#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 3000UL;

static auto Sgm_I2C_Bus = std::make_shared<cpp_bus_driver::HardwareI2c2>(
    SGM41562_SDA, SGM41562_SCL, &Wire);
static auto sgm41562 = std::make_unique<cpp_bus_driver::Sgm41562xx>(
    Sgm_I2C_Bus, SGM41562_ADDRESS);

static bool Sgm_Ready = false;
static uint8_t Sgm_Device_Id = 0;
static uint32_t Next_Heartbeat_Ms = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_sgm41562_test] Serial ready");
    Serial.flush();
}

static void Enable_3V3_Rail()
{
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
    digitalWrite(RT9080_EN, LOW);
    delay(100);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
}

static void Release_Main_I2C()
{
    sgm41562->Deinit();
    Wire.end();
    pinMode(IIC_SDA_1, INPUT);
    pinMode(IIC_SCL_1, INPUT);
}

static void Print_Charger_Config(
    const cpp_bus_driver::Sgm41562xx::ChargerConfig &config)
{
    Serial.printf("[Charger config] charge_enabled=%s high_impedance=%s input_voltage_limit=%umV\r\n",
                  config.charge_enabled ? "yes" : "no",
                  config.high_impedance_enabled ? "yes" : "no",
                  static_cast<unsigned int>(config.input_voltage_limit_mv));
    Serial.printf("[Charger config] input_current_limit_enabled=%s input_current_limit=%umA fast_charge=%umA\r\n",
                  config.input_current_limit_enabled ? "yes" : "no",
                  static_cast<unsigned int>(config.input_current_limit_ma),
                  static_cast<unsigned int>(config.fast_charge_current_ma));
    Serial.printf("[Charger config] termination_current=%umA charge_voltage=%umV system_voltage=%umV\r\n",
                  static_cast<unsigned int>(config.termination_current_ma),
                  static_cast<unsigned int>(config.charge_voltage_limit_mv),
                  static_cast<unsigned int>(config.system_voltage_regulation_mv));
    Serial.printf("[Charger config] watchdog=%s safety_timer=%s termination=%s NTC=%s\r\n",
                  config.watchdog_enabled ? "on" : "off",
                  config.safety_timer_enabled ? "on" : "off",
                  config.charge_termination_enabled ? "on" : "off",
                  config.ntc_enabled ? "on" : "off");
}

static void Print_Irq_Status(
    const cpp_bus_driver::Sgm41562xx::IrqStatus &status)
{
    Serial.printf("[Charger faults] input_power=%d thermal_shutdown=%d battery_overvoltage=%d\r\n",
                  status.input_power_fault,
                  status.thermal_shutdown,
                  status.battery_overvoltage_fault);
    Serial.printf("[Charger faults] safety_timer=%d NTC_overtemperature=%d NTC_cold=%d\r\n",
                  status.safety_timer_expired,
                  status.ntc_hot,
                  status.ntc_cold);
}

static const char *Charge_Status_Name(
    cpp_bus_driver::Sgm41562xx::ChargeStatus status)
{
    using ChargeStatus = cpp_bus_driver::Sgm41562xx::ChargeStatus;
    switch (status)
    {
    case ChargeStatus::kNotCharging:
        return "not_charging";
    case ChargeStatus::kPrecharge:
        return "precharge";
    case ChargeStatus::kCharging:
        return "charging";
    case ChargeStatus::kChargeComplete:
        return "charge_complete";
    default:
        return "unknown";
    }
}

static bool Initialize_Sgm41562()
{
    Serial.println("[Stage] SGM41562 diagnostic starting");
    Serial.println("[Pin map] Main I2C SDA=P1.08 SCL=P0.11 address=0x03");
    Serial.println("[Pin map] SGM41562 INT=P0.16");
    Serial.println("[Safety] Read status and configuration only; do not change charging, shipping mode, or safety timer settings");

    Enable_3V3_Rail();
    pinMode(SGM41562_INT, INPUT);
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence");
    Serial.printf("[Main I2C] Using SDA=P1.08 SCL=P0.11; LoRa DIO1 shares P0.11 and remains disabled\r\n");

    if (!sgm41562->Init())
    {
        Serial.println("[SGM41562] Initialization failed; check 0x03 response, power, and main I2C");
        Release_Main_I2C();
        return false;
    }

    if (!sgm41562->GetDeviceId(Sgm_Device_Id))
    {
        Serial.println("[SGM41562] Device ID read failed");
        Release_Main_I2C();
        return false;
    }
    Serial.printf("[SGM41562] Initialization successful model=%s device_id=0x%02X\r\n",
                  cpp_bus_driver::Sgm41562xx::ChipTypeToString(
                      sgm41562->GetChipType()),
                  static_cast<unsigned int>(Sgm_Device_Id));

    cpp_bus_driver::Sgm41562xx::ChargerConfig config;
    if (sgm41562->GetChargerConfig(config))
    {
        Print_Charger_Config(config);
    }
    else
    {
        Serial.println("[Charger config] Read-only configuration read failed");
        Release_Main_I2C();
        return false;
    }
    return true;
}

static void Read_Sgm41562_Status()
{
    cpp_bus_driver::Sgm41562xx::IrqStatus irq_status;
    const bool irq_ok = sgm41562->GetIrqStatus(irq_status);
    Serial.printf("[SGM41562] IRQ read=%s INT=P0.16 level=%d\r\n",
                  irq_ok ? "success" : "failed",
                  digitalRead(SGM41562_INT));
    if (irq_ok)
    {
        Print_Irq_Status(irq_status);
    }

    cpp_bus_driver::Sgm41562xx::ChipStatus chip_status;
    const bool status_ok = sgm41562->GetChipStatus(chip_status);
    if (!status_ok)
    {
        Serial.println("[SGM41562] Chip status read failed");
    }
    else
    {
        Serial.printf("[Chip status] charge_status=%s input_power_good=%d power_path=%d\r\n",
                      Charge_Status_Name(chip_status.charge_status),
                      chip_status.input_power_good,
                      chip_status.power_path_management_active);
        Serial.printf("[Chip status] watchdog_expired=%d thermal_regulation=%d\r\n",
                      chip_status.watchdog_expired,
                      chip_status.thermal_regulation_active);
    }

}

void setup()
{
    Wait_For_Serial();
    Sgm_Ready = Initialize_Sgm41562();
    Serial.printf("[Result] SGM41562 test=%s\r\n",
                  Sgm_Ready ? "PASS" : "FAIL");
    Next_Heartbeat_Ms = millis();
}

void loop()
{
    if (Sgm_Ready && millis() >= Next_Heartbeat_Ms)
    {
        Read_Sgm41562_Status();
        Serial.printf("[Heartbeat] SGM41562 running device_id=0x%02X\r\n",
                      static_cast<unsigned int>(Sgm_Device_Id));
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    else if (!Sgm_Ready && millis() >= Next_Heartbeat_Ms)
    {
        Serial.println("[Heartbeat] SGM41562 is not ready; read-only diagnostics remain stopped");
        Next_Heartbeat_Ms = millis() + HEARTBEAT_INTERVAL_MS;
    }
    delay(10);
}
