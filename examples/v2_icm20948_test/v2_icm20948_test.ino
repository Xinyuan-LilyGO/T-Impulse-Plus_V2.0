#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>

#include "ICM20948_WE.h"
#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t SENSOR_INTERVAL_MS = 500UL;

ICM20948_WE myIMU(&Wire, ICM20948_ADDRESS);

static bool Imu_Ready = false;
static bool Magnetometer_Ready = false;
static uint32_t Next_Sample_Ms = 0;
static uint32_t Sample_Count = 0;

static void Wait_For_Serial()
{
    Serial.begin(SERIAL_BAUD);
    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_icm20948_test] Serial ready");
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
    Wire.end();
    pinMode(ICM20948_SDA, INPUT);
    pinMode(ICM20948_SCL, INPUT);
}

static bool Check_Main_I2C_Lines()
{
    pinMode(ICM20948_SDA, INPUT_PULLUP);
    pinMode(ICM20948_SCL, INPUT_PULLUP);
    delay(5);
    const int sda_level = digitalRead(ICM20948_SDA);
    const int scl_level = digitalRead(ICM20948_SCL);
    Serial.printf("[Main I2C] SDA=P1.08=%d SCL=P0.11=%d idle=%s\r\n",
                  sda_level,
                  scl_level,
                  (sda_level == HIGH && scl_level == HIGH) ? "yes" : "no");
    return (sda_level == HIGH) && (scl_level == HIGH);
}

static bool Initialize_Imu()
{
    Serial.println("[Stage] ICM20948 diagnostic starting");
    Serial.println("[Pin map] Main I2C SDA=P1.08 SCL=P0.11 address=0x69");
    Serial.println("[Pin map] ICM20948 INT=P0.07");
    Serial.println("[Safety] Only the IMU and main I2C are initialized; display, charger, LoRa, and GNSS stay disabled");

    Enable_3V3_Rail();
    pinMode(ICM20948_INT, INPUT);
    if (!Check_Main_I2C_Lines())
    {
        Serial.println("[Main I2C] Line is low; stopping ICM20948 initialization");
        Release_Main_I2C();
        return false;
    }

    Wire.setPins(ICM20948_SDA, ICM20948_SCL);
    Wire.begin();
    Wire.setClock(100000UL);
    Serial.println("[Main I2C] Wire started clock_hz=100000");

    if (!myIMU.init())
    {
        Serial.println("[ICM20948] AG initialization failed; check 0x69, power, and SDA/SCL");
        Release_Main_I2C();
        return false;
    }
    Imu_Ready = true;
    myIMU.sleep(false);
    Serial.println("[ICM20948] Accelerometer and gyroscope initialized");

    Magnetometer_Ready = myIMU.initMagnetometer();
    Serial.printf("[ICM20948] Magnetometer initialization=%s\r\n",
                  Magnetometer_Ready ? "success" : "failed");
    if (!Magnetometer_Ready)
    {
        myIMU.sleep(true);
        Imu_Ready = false;
        Release_Main_I2C();
        return false;
    }
    Serial.println("[ICM20948] Place the board level and still; starting automatic offset calibration");
    delay(1000);
    myIMU.autoOffsets();
    Serial.println("[ICM20948] Automatic offset calibration complete");
    myIMU.setAccRange(ICM20948_ACC_RANGE_2G);
    myIMU.setAccDLPF(ICM20948_DLPF_6);
    myIMU.setMagOpMode(AK09916_CONT_MODE_20HZ);
    Serial.println("[ICM20948] Measurement configuration complete; hold still, then rotate X/Y/Z slowly");
    return true;
}

static void Print_Sensor_Sample()
{
    myIMU.readSensor();
    const xyzFloat acc_raw = myIMU.getAccRawValues();
    const xyzFloat acc_g = myIMU.getGValues();
    const xyzFloat gyr_raw = myIMU.getGyrRawValues();
    const xyzFloat gyr = myIMU.getGyrValues();
    const xyzFloat angle = myIMU.getAngles();
    const xyzFloat mag = myIMU.getMagValues();

    Serial.printf("[IMU] sample=%lu INT=P0.07 level=%d temperature=%.02fC\r\n",
                  static_cast<unsigned long>(Sample_Count++),
                  digitalRead(ICM20948_INT),
                  myIMU.getTemperature());
    Serial.printf("[IMU] acceleration_raw=(%.0f,%.0f,%.0f) g=(%.03f,%.03f,%.03f)\r\n",
                  acc_raw.x, acc_raw.y, acc_raw.z,
                  acc_g.x, acc_g.y, acc_g.z);
    Serial.printf("[IMU] gyroscope_raw=(%.0f,%.0f,%.0f) dps=(%.03f,%.03f,%.03f)\r\n",
                  gyr_raw.x, gyr_raw.y, gyr_raw.z,
                  gyr.x, gyr.y, gyr.z);
    Serial.printf("[IMU] angles=(%.02f,%.02f,%.02f) pitch=%.02f roll=%.02f\r\n",
                  angle.x, angle.y, angle.z,
                  myIMU.getPitch(), myIMU.getRoll());
    Serial.printf("[IMU] magnetometer=(%.03f,%.03f,%.03f)\r\n",
                  mag.x, mag.y, mag.z);
}

void setup()
{
    Wait_For_Serial();
    const bool initialized = Initialize_Imu();
    Serial.printf("[Result] ICM20948 test=%s (magnetometer=%s)\r\n",
                  initialized ? "PASS" : "FAIL",
                  Magnetometer_Ready ? "ready" : "not_ready");
    Next_Sample_Ms = millis();
}

void loop()
{
    if (Imu_Ready && millis() >= Next_Sample_Ms)
    {
        Print_Sensor_Sample();
        Next_Sample_Ms = millis() + SENSOR_INTERVAL_MS;
    }
    else if (!Imu_Ready && millis() >= Next_Sample_Ms)
    {
        Serial.println("[Heartbeat] ICM20948 is not ready; check main I2C and 0x69 power");
        Next_Sample_Ms = millis() + 2000UL;
    }
    delay(5);
}
