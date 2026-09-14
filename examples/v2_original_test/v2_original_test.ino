#include <SPI.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pin_config.h"
#include "Display_Fonts.h"
#include <bluefruit.h>
#include "Adafruit_SPIFlash.h"
#include "zd25_flash_config.h"
#include "ICM20948_WE.h"
#include "RadioLib.h"
#include "cpp_bus_driver_library.h"
#include "TinyGPSPlus.h"
#include "wiring.h"

#define SOFTWARE_NAME "original_test"
#define SOFTWARE_LASTEDITTIME "202607270912"
#define BOARD_VERSION "v1.0"

#define GNSS_RAW_LINE_BUFFER_SIZE 128

static constexpr uint32_t LORA_IRQ_POLL_INTERVAL_MS = 20UL;
static constexpr uint32_t LORA_STATUS_REPORT_INTERVAL_MS = 5000UL;
static constexpr size_t LORA_PACKET_BUFFER_SIZE = 256;
static constexpr uint32_t LORA_RX_EVENT_IRQ_FLAGS =
    RADIOLIB_SX126X_IRQ_RX_DONE |
    RADIOLIB_SX126X_IRQ_CRC_ERR;

enum class System_Window
{
    HOME = 0,
    FLASH_TEST,
    BATTERY_TEST,
    IMU_TEST,
    GPS_TEST,
    LORA_TEST,

    END,
};

struct BLE_Uart_Operator
{
    using state = enum {
        UNCONNECTED, // not connected
        CONNECTED,   // connected already
    };

    struct
    {
        bool state_flag = state::UNCONNECTED;
        char device_name[32] = {'\0'};
    } connection;

    struct
    {
        uint8_t receive_data[100] = {'\0'};
    } transmission;

    bool initialization_flag = false;
};

struct System_Operator
{
    size_t time = 0;

    uint8_t sleep_count = 0;

    struct
    {
        bool screen = false;
        bool flash = false;
        bool imu = false;
        bool lora = false;

    } init_flag;
};

struct SX1262_Operator
{
    struct
    {
        float value = 868.0;
        bool change_flag = false;
    } frequency;
    struct
    {
        float value = 125.0;
        bool change_flag = false;
    } bandwidth;
    struct
    {
        uint8_t value = 10;
        bool change_flag = false;
    } spreading_factor;
    struct
    {
        uint8_t value = 6;
        bool change_flag = false;
    } coding_rate;
    struct
    {
        uint8_t value = 0xAB;
        bool change_flag = false;
    } sync_word;
    struct
    {
        int8_t value = 22;
        bool change_flag = false;
    } output_power;
    struct
    {
        float value = 140;
        bool change_flag = false;
    } current_limit;
    struct
    {
        int16_t value = 15;
        bool change_flag = false;
    } preamble_length;
    struct
    {
        bool value = false;
        bool change_flag = false;
    } crc;

};

struct Button_Triggered_Operator
{
    using gesture = enum {
        NOT_ACTIVE,   // not active
        SINGLE_CLICK, // single click
        DOUBLE_CLICK, // double click
        LONG_PRESS,   // long press
    };
    const uint32_t button_number = TTP223_KEY;

    bool trigger_level = LOW;

    size_t cycletime_1 = 0;
    size_t cycletime_2 = 0;

    uint8_t current_state = gesture::NOT_ACTIVE;
    bool trigger_start_flag = false;
    bool trigger_flag = false;
    bool timing_flag = false;
    int8_t paragraph_triggered_level = -1;
    uint8_t high_triggered_count = 0;
    uint8_t low_triggered_count = 0;
    uint8_t paragraph_triggered_count = 0;

    volatile bool Interrupt_Flag = false;
};

uint8_t Current_Window_Count = 0;
System_Window Current_Window = System_Window::HOME;

bool Battery_Control_Switch = false;

size_t CycleTime = 0;
size_t CycleTime_2 = 0;
volatile bool receivedFlag = false;
volatile uint32_t dio1EdgeCount = 0;
uint32_t lastIrqPollMs = 0;
uint32_t lastStatusReportMs = 0;
uint32_t lastIrqFlags = 0;
bool LoRa_SPI_Started = false;
bool LoRa_Radio_Started = false;

bool Gps_Positioning_Flag = false;
int32_t Gps_Positioning_Time = 0;

TinyGPSPlus gps;
char Gnss_Raw_Line[GNSS_RAW_LINE_BUFFER_SIZE] = {};
size_t Gnss_Raw_Line_Count = 0;
uint32_t Gnss_Raw_Bytes = 0;
uint32_t Gnss_Sentence_Count = 0;
uint32_t Gnss_Line_Overflow_Count = 0;

/* UART Serivce: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * UART RXD    : 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 * UART TXD    : 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */

// BLE Service
BLEDfu bledfu;   // OTA DFU service
BLEDis bledis;   // device information
BLEUart bleuart; // uart over ble

BLE_Uart_Operator BLE_Uart_Op;
System_Operator System_Op;
Button_Triggered_Operator Button_Triggered_OP;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, SCREEN_RST);

// QSPI
Adafruit_FlashTransport_QSPI flashTransport(ZD25WQ32C_SCLK, ZD25WQ32C_CS,
                                            ZD25WQ32C_IO0, ZD25WQ32C_IO1,
                                            ZD25WQ32C_IO2, ZD25WQ32C_IO3);

Adafruit_SPIFlash flash(&flashTransport);

/* There are several ways to create your ICM20948 object:
 * ICM20948_WE myIMU = ICM20948_WE()              -> uses Wire / I2C Address = 0x68
 * ICM20948_WE myIMU = ICM20948_WE(ICM20948_ADDR) -> uses Wire / ICM20948_ADDR
 * ICM20948_WE myIMU = ICM20948_WE(&wire2)        -> uses the TwoWire object wire2 / ICM20948_ADDR
 * ICM20948_WE myIMU = ICM20948_WE(&wire2, ICM20948_ADDR) -> all together
 * ICM20948_WE myIMU = ICM20948_WE(CS_PIN, spi);  -> uses SPI, spi is just a flag, see SPI example
 * ICM20948_WE myIMU = ICM20948_WE(&SPI, CS_PIN, spi);  -> uses SPI / passes the SPI object, spi is just a flag, see SPI example
 */
ICM20948_WE myIMU = ICM20948_WE(&Wire, ICM20948_ADDRESS);

SX1262_Operator SX1262_OP;

SPIClass Custom_SPI(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);

// AcSiP S62F uses the SX1262 RadioLib compatibility path. RadioLib expects
// the SX1262 version register to report "SX1261", which matches this module.
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI);
static constexpr float S62F_TCXO_VOLTAGE = SX1262_TCXO_VOLTAGE;
static constexpr bool S62F_USE_REGULATOR_LDO = SX1262_USE_REGULATOR_LDO;

auto sgm41562_i2c_bus = std::make_shared<cpp_bus_driver::HardwareI2c2>(
    SGM41562_SDA, SGM41562_SCL, &Wire);

auto sgm41562 = std::make_unique<cpp_bus_driver::Sgm41562xx>(
    sgm41562_i2c_bus, SGM41562_ADDRESS);

void P011_InitializeAsI2C(void)
{
    // Release the radio interrupt before assigning P0.11 back to TWIM.
    radio.clearDio1Action();
    Wire.end();
    pinMode(IIC_SDA_1, INPUT);
    pinMode(IIC_SCL_1, INPUT);
    Wire.setPins(IIC_SDA_1, IIC_SCL_1);
    Wire.begin();
}

void P011_InitializeAsExternalInterrupt(void)
{
    // The radio DIO1 callback maps this released GPIO to a GPIOTE interrupt.
    radio.clearDio1Action();
    Wire.end();
    pinMode(IIC_SDA_1, INPUT);
    pinMode(IIC_SCL_1, INPUT);
    pinMode(SX1262_DIO1, INPUT);
}

void Release_Main_I2C_For_LoRa()
{
    P011_InitializeAsExternalInterrupt();
}

void set_receive_flag(void)
{
    receivedFlag = true;
    dio1EdgeCount++;
}

void log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    Serial.print(buffer);
    if ((BLE_Uart_Op.initialization_flag == true) && (BLE_Uart_Op.connection.state_flag == BLE_Uart_Op.state::CONNECTED))
    {
        bleuart.print(buffer);
    }

    va_end(args);
}

void Process_Gnss_Byte(uint8_t value)
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
        if (gps.location.isValid())
        {
            Gps_Positioning_Flag = true;
            log_printf("[GNSS] Fix valid latitude=%.6f longitude=%.6f\n",
                       gps.location.lat(), gps.location.lng());
        }
        log_printf("[GNSS] NMEA sentences=%lu date_valid=%d time_valid=%d\n",
                   static_cast<unsigned long>(Gnss_Sentence_Count),
                   gps.date.isValid(), gps.time.isValid());
    }

    if (value == '\n')
    {
        Gnss_Raw_Line[Gnss_Raw_Line_Count] = '\0';
        log_printf("[GNSS] Raw data=%s", Gnss_Raw_Line);
        Gnss_Raw_Line_Count = 0;
        Gnss_Raw_Line[0] = '\0';
    }
}

void Vibration_Motor_Trigger(uint32_t delay_ms)
{
    digitalWrite(VIBRATION_MOTOR_DATA, HIGH);
    delay(delay_ms);
    digitalWrite(VIBRATION_MOTOR_DATA, LOW);
}

bool Key_Scanning(void)
{
    if (Button_Triggered_OP.trigger_start_flag == false)
    {
        if (digitalRead(Button_Triggered_OP.button_number) == Button_Triggered_OP.trigger_level)
        {
            Button_Triggered_OP.trigger_start_flag = true;

            log_printf("press button to trigger start");
            Button_Triggered_OP.high_triggered_count = 0;
            Button_Triggered_OP.low_triggered_count = 0;
            Button_Triggered_OP.paragraph_triggered_count = 0;
            Button_Triggered_OP.paragraph_triggered_level = -1;
            Button_Triggered_OP.trigger_flag = true;
            Button_Triggered_OP.timing_flag = true;
        }
    }
    if (Button_Triggered_OP.timing_flag == true)
    {
        Button_Triggered_OP.cycletime_2 = millis() + 1000; // timing 1000ms off
        Button_Triggered_OP.timing_flag = false;
    }
    if (Button_Triggered_OP.trigger_flag == true)
    {
        if (millis() > Button_Triggered_OP.cycletime_1)
        {
            if (digitalRead(Button_Triggered_OP.button_number) == HIGH)
            {
                Button_Triggered_OP.high_triggered_count++;
                if (Button_Triggered_OP.paragraph_triggered_level != HIGH)
                {
                    Button_Triggered_OP.paragraph_triggered_count++;
                    Button_Triggered_OP.paragraph_triggered_level = HIGH;
                }
            }
            else if (digitalRead(Button_Triggered_OP.button_number) == LOW)
            {
                Button_Triggered_OP.low_triggered_count++;
                if (Button_Triggered_OP.paragraph_triggered_level != LOW)
                {
                    Button_Triggered_OP.paragraph_triggered_count++;
                    Button_Triggered_OP.paragraph_triggered_level = LOW;
                }
            }
            Button_Triggered_OP.cycletime_1 = millis() + 50;
        }

        if (Button_Triggered_OP.timing_flag == false)
        {
            if (millis() > Button_Triggered_OP.cycletime_2)
            {
                log_printf("end\n");
                log_printf("high_triggered_count: %d\n", Button_Triggered_OP.high_triggered_count);
                log_printf("low_triggered_count: %d\n", Button_Triggered_OP.low_triggered_count);
                log_printf("paragraph_triggered_count: %d\n", Button_Triggered_OP.paragraph_triggered_count);

                Button_Triggered_OP.trigger_flag = false;
                Button_Triggered_OP.trigger_start_flag = false;

                if ((Button_Triggered_OP.paragraph_triggered_count == 2)) // single click
                {
                    Button_Triggered_OP.current_state = Button_Triggered_OP.gesture::SINGLE_CLICK;
                    log_printf("key triggered: SINGLE_CLICK\n");
                    Vibration_Motor_Trigger(150);
                    return true;
                }
                else if ((Button_Triggered_OP.paragraph_triggered_count == 4)) // double click
                {
                    Button_Triggered_OP.current_state = Button_Triggered_OP.gesture::DOUBLE_CLICK;
                    log_printf("key triggered: DOUBLE_CLICK\n");
                    Vibration_Motor_Trigger(150);
                    delay(100);
                    Vibration_Motor_Trigger(150);
                    return true;
                }
                else if ((Button_Triggered_OP.paragraph_triggered_count == 1)) // long press
                {
                    Button_Triggered_OP.current_state = Button_Triggered_OP.gesture::LONG_PRESS;
                    log_printf("key triggered: LONG_PRESS\n");
                    Vibration_Motor_Trigger(500);
                    return true;
                }
            }
        }
    }

    return false;
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
    // Get the reference to current connection
    BLEConnection *connection = Bluefruit.Connection(conn_handle);

    char central_name[32] = {0};
    connection->getPeerName(central_name, sizeof(central_name));

    Serial.print("Connected to ");
    Serial.println(central_name);

    strcpy(BLE_Uart_Op.connection.device_name, central_name);

    BLE_Uart_Op.connection.state_flag = BLE_Uart_Op.state::CONNECTED;
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
    (void)conn_handle;
    (void)reason;

    Serial.println();
    Serial.print("Disconnected, reason = 0x");
    Serial.println(reason, HEX);
    BLE_Uart_Op.connection.state_flag = BLE_Uart_Op.state::UNCONNECTED;
}

void startAdv(void)
{
    // Advertising packet
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();

    // Include bleuart 128-bit uuid
    Bluefruit.Advertising.addService(bleuart);

    // Secondary Scan Response packet (optional)
    // Since there is no room for 'Name' in Advertising packet
    Bluefruit.ScanResponse.addName();

    /* Start Advertising
     * - Enable auto advertising if disconnected
     * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     * - Timeout for fast mode is 30 seconds
     * - Start(timeout) with timeout = 0 will advertise forever (until connected)
     *
     * For recommended advertising interval
     * https://developer.apple.com/library/content/qa/qa1931/_index.html
     */
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
    Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising after n seconds
}

bool BLE_Uart_Initialization(void)
{
    // Setup the BLE LED to be enabled on CONNECT
    // Note: This is actually the default behavior, but provided
    // here in case you want to control this LED manually via PIN 19
    // Bluefruit.autoConnLed(true);

    // Config the peripheral connection with maximum bandwidth
    // more SRAM required by SoftDevice
    // Note: All config***() function must be called before begin()
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

    if (Bluefruit.begin() == false)
    {
        Serial.println("BLE initialization failed");
        return false;
    }
    Serial.println("BLE initialization successful");

    Bluefruit.setTxPower(8); // Check bluefruit.h for supported values
    // Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

    // To be consistent OTA DFU should be added first if it exists
    bledfu.begin();

    // Configure and Start Device Information Service
    bledis.setManufacturer("LILYGO Industries");
    bledis.setModel("T-Impulse-Plus");
    bledis.begin();

    // Configure and Start BLE Uart Service
    bleuart.begin();

    // Set up and start advertising
    startAdv();

    Serial.println("Please use the BLE debugging tool to connect to the development board.");
    Serial.println("Once connected, enter character(s) that you wish to send");

    return true;
}

bool SX1262_Set_Default_Parameters(String *assertion)
{
    if (radio.setFrequency(SX1262_OP.frequency.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set frequency value";
        return false;
    }
    if (radio.setBandwidth(SX1262_OP.bandwidth.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set bandwidth value";
        return false;
    }
    if (radio.setOutputPower(SX1262_OP.output_power.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set output_power value";
        return false;
    }
    if (radio.setCurrentLimit(SX1262_OP.current_limit.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set current_limit value";
        return false;
    }
    if (radio.setPreambleLength(SX1262_OP.preamble_length.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set preamble_length value";
        return false;
    }
    if (radio.setCRC(SX1262_OP.crc.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set crc value";
        return false;
    }
    if (radio.setSpreadingFactor(SX1262_OP.spreading_factor.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set spreading_factor value";
        return false;
    }
    if (radio.setCodingRate(SX1262_OP.coding_rate.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set coding_rate value";
        return false;
    }
    if (radio.setSyncWord(SX1262_OP.sync_word.value) != RADIOLIB_ERR_NONE)
    {
        *assertion = "Failed to set sync_word value";
        return false;
    }
    return true;
}

void Shutdown_LoRa_Test()
{
    radio.clearDio1Action();
    receivedFlag = false;

    if (LoRa_Radio_Started)
    {
        const int16_t state = radio.sleep();
        if (state != RADIOLIB_ERR_NONE)
        {
            log_printf("[LoRa] Sleep failed, RadioLib error code=%d\n", state);
        }
    }

    if (LoRa_SPI_Started)
    {
        Custom_SPI.end();
    }
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
    System_Op.init_flag.lora = false;
    LoRa_Radio_Started = false;
    LoRa_SPI_Started = false;
    lastIrqPollMs = 0;
    lastStatusReportMs = 0;
    lastIrqFlags = 0;
    P011_InitializeAsI2C();
    log_printf("[LoRa] Callback, SPI, and radio pins released; main I2C restored\n");
}

void Window_Init(System_Window Window)
{
    if (Window == System_Window::LORA_TEST)
    {
        Release_Main_I2C_For_LoRa();
    }
    else
    {
        P011_InitializeAsI2C();
    }

    switch (Window)
    {
    case System_Window::HOME:
        System_Op.sleep_count = 0;
        break;
    case System_Window::FLASH_TEST:
        // Custom_SPI.setClockDivider(SPI_CLOCK_DIV2); // dual frequency 32MHz

        flash.begin(ZD25WQ32_DEVICES, ZD25WQ32_DEVICE_COUNT);
        flashTransport.setClockSpeed(32000000UL, 0);
        flashTransport.runCommand(0xAB); // Exit deep sleep mode

        if (flash.begin(ZD25WQ32_DEVICES, ZD25WQ32_DEVICE_COUNT) == false)
        {
            log_printf("flash init fail\n");
            System_Op.init_flag.flash = false;
        }
        else
        {
            log_printf("flash init successful\n");
            System_Op.init_flag.flash = true;
            log_printf("[Flash] QSPI JEDEC=0x%lX capacity=%lu kbytes\n",
                       static_cast<unsigned long>(flash.getJEDECID()),
                       static_cast<unsigned long>(flash.size() / 1024));
        }

        break;

    case System_Window::BATTERY_TEST:
        if (sgm41562->Init() == false)
        {
            log_printf("Sgm41562 init fail\n");
        }
        else
        {
            log_printf("Sgm41562 init successful\n");
        }

        // Measure battery
        pinMode(BATTERY_ADC_DATA, INPUT);
        pinMode(BATTERY_MEASUREMENT_CONTROL, OUTPUT);
        digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH); // Turn on battery voltage measurement

        Battery_Control_Switch = true;

        // Set the analog reference to 3.0V (default = 3.6V)
        analogReference(AR_INTERNAL_3_0);
        // Set the resolution to 12-bit (0..4095)
        analogReadResolution(12); // Can be 8, 10, 12 or 14

        break;

    case System_Window::IMU_TEST:
        if (myIMU.init() == false)
        {
            log_printf("ICM20948 AG init fail\n");
            System_Op.init_flag.imu = false;
        }
        else
        {
            myIMU.sleep(false);
            if (myIMU.initMagnetometer() == false)
            {
                log_printf("ICM20948 M init fail\n");
                System_Op.init_flag.imu = false;
            }

            log_printf("ICM20948 init successful\n");
            System_Op.init_flag.imu = true;

            /*  This is a method to calibrate. You have to determine the minimum and maximum
             *  raw acceleration values of the axes determined in the range +/- 2 g.
             *  You call the function as follows: setAccOffsets(xMin,xMax,yMin,yMax,zMin,zMax);
             *  The parameters are floats.
             *  The calibration changes the slope / ratio of raw acceleration vs g. The zero point
             *  is set as (min + max)/2.
             */
            // myIMU.setAccOffsets(-16330.0, 16450.0, -16600.0, 16180.0, -16520.0, 16690.0);

            /* The starting point, if you position the ICM20948 flat, is not necessarily 0g/0g/1g for x/y/z.
             * The autoOffset function measures offset. It assumes your ICM20948 is positioned flat with its
             * x,y-plane. The more you deviate from this, the less accurate will be your results.
             * It overwrites the zero points of setAccOffsets, but keeps the correction of the slope.
             * The function also measures the offset of the gyroscope data. The gyroscope offset does not
             * depend on the positioning.
             * This function needs to be called after setAccsOffsets but before other settings since it will
             * overwrite settings!
             * You can query the offsets with the functions:
             * xyzFloat getAccOffsets() and xyzFloat getGyrOffsets()
             * You can apply the offsets using:
             * setAccOffsets(xyzFloat yourOffsets) and setGyrOffsets(xyzFloat yourOffsets)
             */
            log_printf("position your icm20948 flat and don't move it - calibrating...\n");
            delay(1000);
            myIMU.autoOffsets();
            log_printf("done!\n");

            /* enables or disables the acceleration sensor, default: enabled */
            // myIMU.enableAcc(true);

            /*  ICM20948_ACC_RANGE_2G      2 g   (default)
             *  ICM20948_ACC_RANGE_4G      4 g
             *  ICM20948_ACC_RANGE_8G      8 g
             *  ICM20948_ACC_RANGE_16G    16 g
             */
            myIMU.setAccRange(ICM20948_ACC_RANGE_2G);

            /*  Choose a level for the Digital Low Pass Filter or switch it off.
             *  ICM20948_DLPF_0, ICM20948_DLPF_2, ...... ICM20948_DLPF_7, ICM20948_DLPF_OFF
             *
             *  IMPORTANT: This needs to be ICM20948_DLPF_7 if DLPF is used in cycle mode!
             *
             *  DLPF       3dB Bandwidth [Hz]      Output Rate [Hz]
             *    0              246.0               1125/(1+ASRD)
             *    1              246.0               1125/(1+ASRD)
             *    2              111.4               1125/(1+ASRD)
             *    3               50.4               1125/(1+ASRD)
             *    4               23.9               1125/(1+ASRD)
             *    5               11.5               1125/(1+ASRD)
             *    6                5.7               1125/(1+ASRD)
             *    7              473.0               1125/(1+ASRD)
             *    OFF           1209.0               4500
             *
             *    ASRD = Accelerometer Sample Rate Divider (0...4095)
             *    You achieve lowest noise using level 6
             */
            myIMU.setAccDLPF(ICM20948_DLPF_6);

            /*  Acceleration sample rate divider divides the output rate of the accelerometer.
             *  Sample rate = Basic sample rate / (1 + divider)
             *  It can only be applied if the corresponding DLPF is not off!
             *  Divider is a number 0...4095 (different range compared to gyroscope)
             *  If sample rates are set for the accelerometer and the gyroscope, the gyroscope
             *  sample rate has priority.
             */
            // myIMU.setAccSampleRateDivider(10);

            /* You can set the following modes for the magnetometer:
             * AK09916_PWR_DOWN          Power down to save energy
             * AK09916_TRIGGER_MODE      Measurements on request, a measurement is triggered by
             *                           calling setMagOpMode(AK09916_TRIGGER_MODE)
             * AK09916_CONT_MODE_10HZ    Continuous measurements, 10 Hz rate
             * AK09916_CONT_MODE_20HZ    Continuous measurements, 20 Hz rate
             * AK09916_CONT_MODE_50HZ    Continuous measurements, 50 Hz rate
             * AK09916_CONT_MODE_100HZ   Continuous measurements, 100 Hz rate (default)
             */
            myIMU.setMagOpMode(AK09916_CONT_MODE_20HZ);
        }

        break;

    case System_Window::GPS_TEST:
#if GPS_POWER_CONTROL_AVAILABLE
        pinMode(GPS_EN, OUTPUT);
        digitalWrite(GPS_EN, HIGH);
        delay(100);
#endif
        Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
        Serial2.begin(38400);

        digitalWrite(GPS_EN, LOW); // GNSS module enters the receive phase

        Gps_Positioning_Flag = false;
        Gps_Positioning_Time = 0;
        Gnss_Raw_Line_Count = 0;
        Gnss_Raw_Bytes = 0;
        Gnss_Sentence_Count = 0;
        Gnss_Line_Overflow_Count = 0;

        break;
    case System_Window::LORA_TEST:
    {
        log_printf("[LoRa] P0.11 shares main-I2C SCL with SX1262 DIO1 conflict=%u; release main I2C first\n",
                   static_cast<unsigned int>(V2_LORA_DIO1_I2C_SCL_CONFLICT));
        System_Op.init_flag.lora = false;
        receivedFlag = false;
        dio1EdgeCount = 0;
        lastIrqPollMs = 0;
        lastStatusReportMs = 0;
        lastIrqFlags = 0;
        Custom_SPI.begin();
        LoRa_SPI_Started = true;
        Custom_SPI.setClockDivider(SPI_CLOCK_DIV2);

        int16_t state = radio.begin(
            SX1262_OP.frequency.value,
            SX1262_OP.bandwidth.value,
            SX1262_OP.spreading_factor.value,
            SX1262_OP.coding_rate.value,
            SX1262_OP.sync_word.value,
            SX1262_OP.output_power.value,
            SX1262_OP.preamble_length.value,
            S62F_TCXO_VOLTAGE, S62F_USE_REGULATOR_LDO);
        if (state != RADIOLIB_ERR_NONE)
        {
            log_printf("[LoRa] SX1262 initialization failed, RadioLib error code=%d\n", state);
            Shutdown_LoRa_Test();
        }
        else
        {
            LoRa_Radio_Started = true;
            log_printf("[LoRa] SX1262 initialization successful, frequency=%.03f MHz\n",
                       SX1262_OP.frequency.value);
            System_Op.init_flag.lora = true;

            radio.setRfSwitchPins(SX1262_RF_VC2, SX1262_RF_VC1);

            radio.setPacketReceivedAction(set_receive_flag);

            String buffer_str;
            if (SX1262_Set_Default_Parameters(&buffer_str) == false)
            {
                log_printf("[LoRa] Parameter setup failed, assertion=%s\n", buffer_str.c_str());
                Shutdown_LoRa_Test();
            }

            if (System_Op.init_flag.lora == false)
            {
                break;
            }

            state = radio.startReceive();
            if (state != RADIOLIB_ERR_NONE)
            {
                log_printf("[LoRa] Start receive failed, RadioLib error code=%d\n", state);
                Shutdown_LoRa_Test();
                break;
            }

            System_Op.init_flag.lora = true;
            lastIrqPollMs = millis();
            lastIrqFlags = radio.getIrqFlags();

            display.clearDisplay();
            display.setCursor(0, 0);
            display.printf("Lora Y RX");
            display.display();
        }

        break;
    }

    default:
        break;
    }
}

void Window_End(System_Window Window)
{
    switch (Window)
    {
    case System_Window::HOME:
        CycleTime = 0;
        break;
    case System_Window::FLASH_TEST:
        flashTransport.runCommand(0xB9); // Flash Deep Sleep
        flash.end();
        pinMode(ZD25WQ32C_SCLK, INPUT);
        pinMode(ZD25WQ32C_CS, INPUT);
        pinMode(ZD25WQ32C_IO0, INPUT);
        pinMode(ZD25WQ32C_IO1, INPUT);
        pinMode(ZD25WQ32C_IO2, INPUT);
        pinMode(ZD25WQ32C_IO3, INPUT);
        nrf_gpio_cfg_default(ZD25WQ32C_SCLK);
        nrf_gpio_cfg_default(ZD25WQ32C_CS);
        nrf_gpio_cfg_default(ZD25WQ32C_IO0);
        nrf_gpio_cfg_default(ZD25WQ32C_IO1);
        nrf_gpio_cfg_default(ZD25WQ32C_IO2);
        nrf_gpio_cfg_default(ZD25WQ32C_IO3);
        CycleTime = 0;
        break;
    case System_Window::BATTERY_TEST:
        digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW); // Turn off battery voltage measurement
        pinMode(BATTERY_MEASUREMENT_CONTROL, INPUT);
        nrf_gpio_cfg_default(BATTERY_MEASUREMENT_CONTROL);
        nrf_gpio_cfg_default(BATTERY_ADC_DATA);
        Battery_Control_Switch = false;
        CycleTime = 0;
        break;
    case System_Window::IMU_TEST:
        myIMU.sleep(true);
        pinMode(ICM20948_SDA, INPUT);
        pinMode(ICM20948_SCL, INPUT);
        nrf_gpio_cfg_default(ICM20948_SDA);
        nrf_gpio_cfg_default(ICM20948_SCL);
        CycleTime = 0;
        break;
    case System_Window::GPS_TEST:
        Serial2.end();
        digitalWrite(GPS_EN, HIGH); // GNSS module off
        nrf_gpio_cfg_default(GPS_UART_TX);
        nrf_gpio_cfg_default(GPS_UART_RX);
        CycleTime = 0;
        break;
    case System_Window::LORA_TEST:
        Shutdown_LoRa_Test();
        CycleTime = 0;
        break;

    default:
        break;
    }
}

void System_Sleep(bool mode)
{
    if (mode == true)
    {
        // DISPLAYOFF is required before releasing Wire1; cutting the rail
        // alone does not guarantee that the OLED panel stops driving.
        if (System_Op.init_flag.screen == true)
        {
            display.ssd1306_command(SSD1306_DISPLAYOFF);
            delay(10);
        }

        Wire1.end();
        pinMode(SCREEN_SDA, INPUT);
        pinMode(SCREEN_SCL, INPUT);
        nrf_gpio_cfg_default(SCREEN_SDA);
        nrf_gpio_cfg_default(SCREEN_SCL);
        System_Op.init_flag.screen = false;

        pinMode(VIBRATION_MOTOR_DATA, INPUT);
        nrf_gpio_cfg_default(VIBRATION_MOTOR_DATA);

        // UARTE must be stopped before sleep; leaving it enabled costs about
        // 900 uA on nRF52 even when no GNSS data is being received.
        if (Serial2)
        {
            Serial2.end();
        }
        pinMode(GPS_UART_TX, INPUT);
        pinMode(GPS_UART_RX, INPUT);
        nrf_gpio_cfg_default(GPS_UART_TX);
        nrf_gpio_cfg_default(GPS_UART_RX);
        // Disable the battery divider, then release its control GPIO. Holding
        // this V2 net as an output during normal sleep can create board-level
        // leakage through the external measurement switch.
        pinMode(BATTERY_MEASUREMENT_CONTROL, OUTPUT);
        digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW);
        pinMode(BATTERY_MEASUREMENT_CONTROL, INPUT);
        pinMode(BATTERY_ADC_DATA, INPUT);
        nrf_gpio_cfg_default(BATTERY_MEASUREMENT_CONTROL);
        nrf_gpio_cfg_default(BATTERY_ADC_DATA);
        Battery_Control_Switch = false;

        pinMode(GPS_EN, OUTPUT);
        digitalWrite(GPS_EN, HIGH); // GNSS module off

        sgm41562->Deinit();
        // The main bus is shared by the IMU, charger and LoRa DIO1 net.
        // Leaving TWIM enabled keeps its peripheral clock and pin drivers
        // alive even after the charger object has been deinitialized.
        Wire.end();
        pinMode(IIC_SDA_1, INPUT);
        pinMode(IIC_SCL_1, INPUT);
        pinMode(SGM41562_SDA, INPUT);
        pinMode(SGM41562_SCL, INPUT);
        // pinMode(INPUT) clears the pull-up but leaves the input buffer on.
        // Do this after all aliases of the shared pins; nrf_gpio_cfg_default()
        // handles the encoded P0/P1 pin number and disconnects both inputs.
        nrf_gpio_cfg_default(IIC_SDA_1);
        nrf_gpio_cfg_default(IIC_SCL_1);

        // First disable the regulator, then stop driving the enable net.
        // The board pull-down keeps RT9080 disabled without sinking current
        // through any external pull-up on P0.19.
        digitalWrite(RT9080_EN, LOW);
        pinMode(RT9080_EN, INPUT_PULLDOWN);

        // GPS_EN drives the gate of the GPS_VDD load switch. Once GPS_VDD is
        // off, holding P0.24 high feeds the gate pull-up through R30/R31.
        // Release the pin after cutting the rail so sleep current is not
        // dominated by that backfeed path.
        pinMode(GPS_EN, INPUT);
        nrf_gpio_cfg_default(GPS_EN);
        pinMode(SX1262_DIO2, INPUT);
        nrf_gpio_cfg_default(SX1262_DIO2);

        // Stop BLE before entering the low-power wait loop.
        Bluefruit.Advertising.stop();
        Serial.flush();
        Serial.end();
    }
    else
    {
        // 3.3V Power ON
        pinMode(RT9080_EN, OUTPUT);
        digitalWrite(RT9080_EN, HIGH);
        delay(100);
        digitalWrite(RT9080_EN, LOW);
        delay(100);
        digitalWrite(RT9080_EN, HIGH);
        delay(100);

        if (sgm41562->Init() == false)
        {
            log_printf("Sgm41562 init fail\n");
        }
        else
        {
            log_printf("Sgm41562 init successful\n");

            if (!sgm41562->SetShippingModeDelay(
                    cpp_bus_driver::Sgm41562xx::ShippingModeDelay::k1Second))
            {
                log_printf("Sgm41562 set shipping mode delay fail\n");
            }
        }

        Serial.begin(115200);

        Bluefruit.Advertising.start(0); // 0 = Don't stop advertising after n seconds

        pinMode(VIBRATION_MOTOR_DATA, OUTPUT);
        digitalWrite(VIBRATION_MOTOR_DATA, LOW);

        pinMode(GPS_EN, OUTPUT);
        digitalWrite(GPS_EN, HIGH); // GNSS module off

        Wire1.setPins(SCREEN_SDA, SCREEN_SCL);
        Wire1.begin();
        if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS) == false)
        {
            log_printf("ssd1306 init fail\n");
            System_Op.init_flag.screen = false;
        }
        else
        {
            log_printf("ssd1306 init successful\n");
            System_Op.init_flag.screen = true;
        }
    }
}

void Enter_System_Off_No_Wake(void)
{
    // System OFF must not retain a GPIO sense configuration. In particular,
    // TTP223 is powered from the switched rail and is not a power-off wake
    // source; keeping its active-low sense enabled also adds leakage.
    nrf_gpio_cfg_default(TTP223_KEY);
    nrf_gpio_cfg_default(SGM41562_INT);
    nrf_gpio_cfg_default(ICM20948_INT);
    nrf_gpio_cfg_default(GPS_1PPS);
    nrf_gpio_cfg_default(SX1262_DIO1);
    nrf_gpio_cfg_default(SX1262_DIO2);
    nrf_gpio_cfg_default(SX1262_BUSY);

    // No wake source is configured. Match the Arduino core's System OFF
    // entry: SoftDevice must use its supervisor call, while a standalone
    // nRF52840 can write the POWER register directly.
    uint8_t softdevice_enabled = 0;
    (void)sd_softdevice_is_enabled(&softdevice_enabled);

    if (softdevice_enabled != 0)
    {
        (void)sd_power_system_off();
    }
    else
    {
        NRF_POWER->SYSTEMOFF = 1;
    }

    // The charger shipping mode has already disconnected the switched
    // system rail before this point.
    __DSB();
    while (true)
    {
        __WFE();
    }
}

void setup()
{
    Serial.begin(115200);
    const uint32_t serial_wait_start = millis();
    while (!Serial && (millis() - serial_wait_start < 3000UL))
    {
        delay(10);
    }
    Serial.println("[T-Impulse_Plus_v2.0][v2_original_test] Serial ready");

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
    digitalWrite(RT9080_EN, LOW);
    delay(100);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);

    Wire1.setPins(SCREEN_SDA, SCREEN_SCL);
    Wire1.begin();
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS) == false)
    {
        log_printf("ssd1306 init fail\n");
        System_Op.init_flag.screen = false;
    }
    else
    {
        log_printf("ssd1306 init successful\n");
        System_Op.init_flag.screen = true;
    }

    display.setOffsetCursor(32, 32);
    display.clearDisplay();
    display.fillScreen(WHITE);
    display.setTextColor(BLACK);
    display.setCursor(15, 10);
    display.printf("LILYGO");
    display.display();

    display.setTextColor(WHITE);

    delay(1000);

    Serial.println("[Stage] V2 integrated window initialization");
    Serial.println("[Pin map] Screen I2C SDA=P1.06 SCL=P1.04; main I2C SDA=P1.08 SCL=P0.11");
    Serial.println("[Pin map] GNSS TX=P0.02 RX=P1.15 GPS_EN=P0.24; battery control=P0.17 ADC=P0.05");
    Serial.println("[Safety] P0.24 is only the GPS_EN control net; measure GPS_VDD externally");

    pinMode(TTP223_KEY, INPUT);

    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, HIGH); // GNSS module off

    pinMode(VIBRATION_MOTOR_DATA, OUTPUT);
    digitalWrite(VIBRATION_MOTOR_DATA, LOW);

    if (BLE_Uart_Initialization() == true)
    {
        BLE_Uart_Op.initialization_flag = true;
        log_printf("BLE_Uart_Initialization successful\n");
    }
    else
    {
        BLE_Uart_Op.initialization_flag = false;
        log_printf("BLE_Uart_Initialization fail\n");
    }

    if (sgm41562->Init() == false)
    {
        log_printf("Sgm41562 init fail\n");
    }
    else
    {
        log_printf("Sgm41562 init successful\n");

        if (!sgm41562->SetShippingModeDelay(
                cpp_bus_driver::Sgm41562xx::ShippingModeDelay::k1Second))
        {
            log_printf("Sgm41562 set shipping mode delay fail\n");
        }
    }

    flash.begin(ZD25WQ32_DEVICES, ZD25WQ32_DEVICE_COUNT);
    flashTransport.setClockSpeed(32000000UL, 0);
    flashTransport.runCommand(0xAB); // Exit deep sleep mode
    if (flash.begin(ZD25WQ32_DEVICES, ZD25WQ32_DEVICE_COUNT) == false)
    {
        log_printf("flash init fail\n");
        System_Op.init_flag.flash = false;
    }
    else
    {
        log_printf("flash init successful\n");
        System_Op.init_flag.flash = true;
    }
    flashTransport.runCommand(0xB9); // Flash Deep Sleep
    flash.end();
    pinMode(ZD25WQ32C_SCLK, INPUT);
    pinMode(ZD25WQ32C_CS, INPUT);
    pinMode(ZD25WQ32C_IO0, INPUT);
    pinMode(ZD25WQ32C_IO1, INPUT);
    pinMode(ZD25WQ32C_IO2, INPUT);
    pinMode(ZD25WQ32C_IO3, INPUT);
    nrf_gpio_cfg_default(ZD25WQ32C_SCLK);
    nrf_gpio_cfg_default(ZD25WQ32C_CS);
    nrf_gpio_cfg_default(ZD25WQ32C_IO0);
    nrf_gpio_cfg_default(ZD25WQ32C_IO1);
    nrf_gpio_cfg_default(ZD25WQ32C_IO2);
    nrf_gpio_cfg_default(ZD25WQ32C_IO3);

    P011_InitializeAsI2C();
    if (myIMU.init() == false)
    {
        log_printf("ICM20948 AG init fail\n");
        System_Op.init_flag.imu = false;
    }
    else
    {
        myIMU.sleep(false);
        if (myIMU.initMagnetometer() == false)
        {
            log_printf("ICM20948 M init fail\n");
            System_Op.init_flag.imu = false;
        }

        log_printf("ICM20948 init successful\n");
        System_Op.init_flag.imu = true;

        /*  This is a method to calibrate. You have to determine the minimum and maximum
         *  raw acceleration values of the axes determined in the range +/- 2 g.
         *  You call the function as follows: setAccOffsets(xMin,xMax,yMin,yMax,zMin,zMax);
         *  The parameters are floats.
         *  The calibration changes the slope / ratio of raw acceleration vs g. The zero point
         *  is set as (min + max)/2.
         */
        // myIMU.setAccOffsets(-16330.0, 16450.0, -16600.0, 16180.0, -16520.0, 16690.0);

        /* The starting point, if you position the ICM20948 flat, is not necessarily 0g/0g/1g for x/y/z.
         * The autoOffset function measures offset. It assumes your ICM20948 is positioned flat with its
         * x,y-plane. The more you deviate from this, the less accurate will be your results.
         * It overwrites the zero points of setAccOffsets, but keeps the correction of the slope.
         * The function also measures the offset of the gyroscope data. The gyroscope offset does not
         * depend on the positioning.
         * This function needs to be called after setAccsOffsets but before other settings since it will
         * overwrite settings!
         * You can query the offsets with the functions:
         * xyzFloat getAccOffsets() and xyzFloat getGyrOffsets()
         * You can apply the offsets using:
         * setAccOffsets(xyzFloat yourOffsets) and setGyrOffsets(xyzFloat yourOffsets)
         */
        log_printf("position your icm20948 flat and don't move it - calibrating...\n");
        delay(1000);
        myIMU.autoOffsets();
        log_printf("done!\n");

        /* enables or disables the acceleration sensor, default: enabled */
        // myIMU.enableAcc(true);

        /*  ICM20948_ACC_RANGE_2G      2 g   (default)
         *  ICM20948_ACC_RANGE_4G      4 g
         *  ICM20948_ACC_RANGE_8G      8 g
         *  ICM20948_ACC_RANGE_16G    16 g
         */
        myIMU.setAccRange(ICM20948_ACC_RANGE_2G);

        /*  Choose a level for the Digital Low Pass Filter or switch it off.
         *  ICM20948_DLPF_0, ICM20948_DLPF_2, ...... ICM20948_DLPF_7, ICM20948_DLPF_OFF
         *
         *  IMPORTANT: This needs to be ICM20948_DLPF_7 if DLPF is used in cycle mode!
         *
         *  DLPF       3dB Bandwidth [Hz]      Output Rate [Hz]
         *    0              246.0               1125/(1+ASRD)
         *    1              246.0               1125/(1+ASRD)
         *    2              111.4               1125/(1+ASRD)
         *    3               50.4               1125/(1+ASRD)
         *    4               23.9               1125/(1+ASRD)
         *    5               11.5               1125/(1+ASRD)
         *    6                5.7               1125/(1+ASRD)
         *    7              473.0               1125/(1+ASRD)
         *    OFF           1209.0               4500
         *
         *    ASRD = Accelerometer Sample Rate Divider (0...4095)
         *    You achieve lowest noise using level 6
         */
        myIMU.setAccDLPF(ICM20948_DLPF_6);

        /*  Acceleration sample rate divider divides the output rate of the accelerometer.
         *  Sample rate = Basic sample rate / (1 + divider)
         *  It can only be applied if the corresponding DLPF is not off!
         *  Divider is a number 0...4095 (different range compared to gyroscope)
         *  If sample rates are set for the accelerometer and the gyroscope, the gyroscope
         *  sample rate has priority.
         */
        // myIMU.setAccSampleRateDivider(10);

        /* You can set the following modes for the magnetometer:
         * AK09916_PWR_DOWN          Power down to save energy
         * AK09916_TRIGGER_MODE      Measurements on request, a measurement is triggered by
         *                           calling setMagOpMode(AK09916_TRIGGER_MODE)
         * AK09916_CONT_MODE_10HZ    Continuous measurements, 10 Hz rate
         * AK09916_CONT_MODE_20HZ    Continuous measurements, 20 Hz rate
         * AK09916_CONT_MODE_50HZ    Continuous measurements, 50 Hz rate
         * AK09916_CONT_MODE_100HZ   Continuous measurements, 100 Hz rate (default)
         */
        myIMU.setMagOpMode(AK09916_CONT_MODE_20HZ);
    }
    myIMU.sleep(true);
    pinMode(ICM20948_SDA, INPUT);
    pinMode(ICM20948_SCL, INPUT);
    nrf_gpio_cfg_default(ICM20948_SDA);
    nrf_gpio_cfg_default(ICM20948_SCL);

    // P0.11 is kept on the main I2C bus during boot. LoRa is initialized
    // only after Window_Init(LORA_TEST) releases this shared net.
    System_Op.init_flag.lora = false;
    P011_InitializeAsI2C();

    Window_Init(Current_Window);

    Vibration_Motor_Trigger(150);
}

void loop()
{
    if (Key_Scanning() == true)
    {
        switch (Button_Triggered_OP.current_state)
        {
        case Button_Triggered_OP.gesture::SINGLE_CLICK:

            switch (Current_Window)
            {
            case System_Window::HOME:
                System_Op.sleep_count = 0;
                break;
            case System_Window::BATTERY_TEST:
                delay(300);

                Battery_Control_Switch = !Battery_Control_Switch;
                if (Battery_Control_Switch == true)
                {
                    digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH); // Enable battery voltage measurement
                    log_printf("turn on battery voltage measurement\n");
                }
                else
                {
                    digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW); // Turn off battery voltage measurement
                    log_printf("turn off battery voltage measurement\n");
                }
                break;
            case System_Window::LORA_TEST:
                if (System_Op.init_flag.lora == true)
                {
                    log_printf("[SX1262] receive-only mode\n");
                }
                break;

            default:
                break;
            }

            break;
        case Button_Triggered_OP.gesture::DOUBLE_CLICK:
            if (Current_Window == System_Window::HOME)
            {
                display.clearDisplay();
                display.setCursor(0, 10);
                // display.printf("deep sleep");
                display.printf("power off");
                display.display();

                // log_printf("deep sleep\n");
                log_printf("power off\n");

                delay(1000);

                if (System_Op.init_flag.screen == true)
                {
                    display.ssd1306_command(SSD1306_DISPLAYOFF);
                    delay(10);
                }

                if (!sgm41562->SetShippingModeEnable(true))
                {
                    log_printf("shipping mode enable fail\n");
                }
                else
                {
                    // SGM41562 applies shipping mode after the configured
                    // delay. Keep 3.3 V alive until that timer has expired.
                    delay(1200);
                }

                System_Sleep(true);
                // True power-off: do not configure TTP223 or any other wake
                // source. Re-apply power externally to restart the board.
                Enter_System_Off_No_Wake();
            }
            break;
        case Button_Triggered_OP.gesture::LONG_PRESS:
            Window_End(Current_Window);

            Current_Window_Count++;
            if (Current_Window_Count >= (uint8_t)System_Window::END)
            {
                Current_Window_Count = 0;
            }

            Current_Window = (System_Window)Current_Window_Count;

            Window_Init(Current_Window);
            break;

        default:
            break;
        }
    }

    switch (Current_Window)
    {
    case System_Window::HOME:
    {
        if (millis() > CycleTime)
        {
            System_Op.time = millis();

            display.clearDisplay();
            display.setCursor(17, 0);
            display.printf("T-I-P");
            log_printf("T-Impulse-Plus\n");

            display.setCursor(8, 15);
            uint32_t total_seconds = System_Op.time / 1000;
            uint8_t hours = total_seconds / 3600;
            uint8_t minutes = (total_seconds % 3600) / 60;
            uint8_t seconds = total_seconds % 60;
            display.printf("%02d:%02d:%02d", hours, minutes, seconds);
            log_printf("system time: %02d:%02d:%02d\n", hours, minutes, seconds);

            log_printf(("[T-Impulse_Plus_" + (String)BOARD_VERSION "][" + (String)SOFTWARE_NAME +
                        "]_firmware_" + (String)SOFTWARE_LASTEDITTIME + "\n")
                           .c_str());

            display.display();

            System_Op.sleep_count++;
            if (System_Op.sleep_count > 10)
            {
                display.clearDisplay();
                display.setCursor(0, 10);
                display.printf("light sleep");
                display.display();

                log_printf("light sleep\n");

                delay(1000);

                System_Sleep(true);

                while (true)
                {
                    waitForEvent();
                    delay(1000);

                    if (digitalRead(TTP223_KEY) == LOW)
                    {
                        System_Sleep(false);

                        display.clearDisplay();
                        display.setCursor(10, 10);
                        display.printf("wake up");
                        display.display();

                        log_printf("wake up\n");

                        Window_Init(Current_Window);

                        Vibration_Motor_Trigger(150);

                        delay(1000);
                        break;
                    }
                }
            }

            waitForEvent();

            CycleTime = millis() + 1000;
        }
        break;
    }
    case System_Window::FLASH_TEST:
        if (millis() > CycleTime)
        {
            display.clearDisplay();
            display.setCursor(0, 0);

            if (System_Op.init_flag.flash == true)
            {
                display.printf("Flash Y");
                display.setCursor(0, 10);
                display.printf("%#X", flash.getJEDECID());
                display.setCursor(0, 20);
                display.printf("%dkbytes", flash.size() / 1024);

                log_printf("flash init successful\n");
                log_printf("id: %#X\n", flash.getJEDECID());
                log_printf("size: %d kbytes\n", flash.size() / 1024);
            }
            else
            {
                display.printf("Flash N");

                log_printf("flash init fail\n");
            }

            display.display();

            CycleTime = millis() + 1000;
        }
        break;

    case System_Window::BATTERY_TEST:

        if (millis() > CycleTime)
        {
            float battery_voltage = (((float)analogRead(BATTERY_ADC_DATA) * ((3000.0 / 4096.0))) / 1000.0) * 2.0;

            log_printf("---battery---\n");

            display.clearDisplay();
            display.setCursor(0, 0);
            display.printf("Battery");
            display.setCursor(0, 10);
            if (Battery_Control_Switch == true)
            {
                display.printf("Switch:ON");
                log_printf("battery switch: on\n");
            }
            else
            {
                display.printf("Switch:OFF");
                log_printf("battery switch: off\n");
            }
            display.setCursor(0, 20);
            display.printf("%.03fv", battery_voltage);

            display.display();

            log_printf("battery voltage: %.03f v\n", battery_voltage);

            log_printf("---sgm41562---\n");

            cpp_bus_driver::Sgm41562xx::IrqStatus irq_status;
            if (sgm41562->GetIrqStatus(irq_status))
            {
                log_printf("irq status:\n");
                log_printf(
                    "  input power fault: %d\n", irq_status.input_power_fault);
                log_printf("  battery over voltage fault: %d\n",
                    irq_status.battery_overvoltage_fault);
            }
            else
            {
                log_printf("failed to read irq status\n");
            }

            cpp_bus_driver::Sgm41562xx::ChipStatus chip_status;
            if (sgm41562->GetChipStatus(chip_status))
            {
                log_printf("chip status:\n");
                log_printf("  charge status: ");
                switch (chip_status.charge_status)
                {
                case cpp_bus_driver::Sgm41562xx::ChargeStatus::kNotCharging:
                    log_printf("not_charging\n");
                    break;
                case cpp_bus_driver::Sgm41562xx::ChargeStatus::kPrecharge:
                    log_printf("precharge\n");
                    break;
                case cpp_bus_driver::Sgm41562xx::ChargeStatus::kCharging:
                    log_printf("charge\n");
                    break;
                case cpp_bus_driver::Sgm41562xx::ChargeStatus::kChargeComplete:
                    log_printf("charge_done\n");
                    break;
                default:
                    log_printf("unknown\n");
                    break;
                }
                log_printf("  input power status: %d\n",
                    chip_status.input_power_good);
            }
            else
            {
                log_printf("failed to read chip status\n");
            }
            log_printf("\n");

            CycleTime = millis() + 1000;
        }

        break;
    case System_Window::IMU_TEST:
        if (millis() > CycleTime)
        {
            if (System_Op.init_flag.imu == true)
            {
                myIMU.readSensor();
                xyzFloat gValue = myIMU.getGValues();
                xyzFloat angle = myIMU.getAngles();
                float pitch = myIMU.getPitch();
                float roll = myIMU.getRoll();

                // obtain the x and y values of the magnetometer to calculate the heading angle (Yaw)
                xyzFloat magValues = myIMU.getMagValues();
                float yaw = atan2(magValues.y, magValues.x) * (180.0 / M_PI); // calculate heading angle

                display.clearDisplay();
                display.setCursor(0, 0);
                display.printf("Imu Y p:%.01f", pitch);
                display.setCursor(0, 10);
                display.printf("r:%.01f", roll);
                display.setCursor(0, 20);
                display.printf("y:%.01f", yaw);

                display.display();

                log_printf("imu init successful\n");
                log_printf("pitch = %.6f  |  roll = %.6f  |  yaw = %.6f\n", pitch, roll, yaw);

                CycleTime = millis() + 500;
            }
            else
            {
                display.clearDisplay();
                display.setCursor(0, 0);
                display.printf("Imu N");

                display.display();

                log_printf("imu init fail\n");

                CycleTime = millis() + 1000;
            }
        }
        break;

    case System_Window::GPS_TEST:
        while (Serial2.available() > 0)
        {
            Process_Gnss_Byte(static_cast<uint8_t>(Serial2.read()));
        }

        if (millis() > CycleTime)
        {
            display.clearDisplay();
            display.setCursor(0, 0);
            display.printf("Gps %s:%lds", Gps_Positioning_Flag ? "Y" : "N",
                           static_cast<long>(Gps_Positioning_Time));
            if (gps.location.isValid())
            {
                display.setCursor(0, 15);
                display.printf("%.5f", gps.location.lat());
                display.setCursor(0, 30);
                display.printf("%.5f", gps.location.lng());
            }
            else
            {
                display.setCursor(0, 20);
                display.print("no fix");
            }
            display.display();

            log_printf("[GNSS] status raw_bytes=%lu sentences=%lu line_overflow=%lu GPS_EN=%d measure_GPS_VDD\n",
                       static_cast<unsigned long>(Gnss_Raw_Bytes),
                       static_cast<unsigned long>(Gnss_Sentence_Count),
                       static_cast<unsigned long>(Gnss_Line_Overflow_Count),
                       digitalRead(GPS_EN));
            if (!gps.location.isValid())
            {
                log_printf("[GNSS] No fix yet; no fix does not prove UART failure. Check GPS_VDD and antenna conditions\n");
            }
            Gps_Positioning_Time += 5;
            CycleTime = millis() + 5000;
        }

        break;

    case System_Window::LORA_TEST:
        if (System_Op.init_flag.lora == true)
        {
            const uint32_t now = millis();
            const bool dio1Event = receivedFlag;
            bool receiveEvent = dio1Event;

            // P0.11 is shared with the main-I2C SCL net. If its external
            // interrupt edge is lost, the SX1262 IRQ status still lets us
            // service the receive event over SPI.
            if (now - lastIrqPollMs >= LORA_IRQ_POLL_INTERVAL_MS)
            {
                lastIrqPollMs = now;
                lastIrqFlags = radio.getIrqFlags();
                receiveEvent = receiveEvent ||
                                ((lastIrqFlags & LORA_RX_EVENT_IRQ_FLAGS) != 0);
            }

            if (now - lastStatusReportMs >= LORA_STATUS_REPORT_INTERVAL_MS)
            {
                lastStatusReportMs = now;
                log_printf("[SX1262] Listening irq=0x%lX DIO1=%d BUSY=%d VC1=%d VC2=%d DIO1_edges=%lu\n",
                           static_cast<unsigned long>(lastIrqFlags),
                           digitalRead(SX1262_DIO1),
                           digitalRead(SX1262_BUSY),
                           digitalRead(SX1262_RF_VC1),
                           digitalRead(SX1262_RF_VC2),
                           static_cast<unsigned long>(dio1EdgeCount));
            }

            if (!receiveEvent)
            {
                break;
            }

            receivedFlag = false;
            const uint32_t eventFlags = radio.getIrqFlags();
            lastIrqFlags = eventFlags;
            const size_t packetLength = radio.getPacketLength();
            const size_t readLength = packetLength > 0 && packetLength < LORA_PACKET_BUFFER_SIZE
                                           ? packetLength
                                           : LORA_PACKET_BUFFER_SIZE - 1;
            uint8_t packet[LORA_PACKET_BUFFER_SIZE] = {};

            log_printf("[LoRa] Receive event source=%s packetLength=%lu\n",
                       dio1Event ? "DIO1" : "SPI-poll",
                       static_cast<unsigned long>(packetLength));

            const int16_t readState = radio.readData(packet, readLength);

            if (readState == RADIOLIB_ERR_NONE)
            {
                log_printf("[LoRa] Raw received data=");
                for (size_t index = 0; index < (packetLength < readLength ? packetLength : readLength); index++)
                {
                    log_printf("%02X ", packet[index]);
                }
                log_printf("\n");

                display.clearDisplay();
                display.setCursor(0, 0);
                display.printf("Lora Y RX");
                display.setCursor(0, 10);
                for (size_t index = 0; index < (packetLength < readLength ? packetLength : readLength); index++)
                {
                    if (packet[index] >= 32 && packet[index] <= 126)
                    {
                        display.print(static_cast<char>(packet[index]));
                    }
                }
                display.display();

                log_printf("[LoRa] RSSI=%.2f dBm SNR=%.2f dB frequency_error=%.2f Hz\n",
                           radio.getRSSI(), radio.getSNR(),
                           radio.getFrequencyError());
            }
            else if (readState == RADIOLIB_ERR_CRC_MISMATCH ||
                     (eventFlags & RADIOLIB_SX126X_IRQ_CRC_ERR) != 0)
            {
                log_printf("[LoRa] CRC error, RadioLib error code=%d\n", readState);
            }
            else
            {
                log_printf("[LoRa] Read receive data failed, RadioLib error code=%d\n", readState);
            }

            const int16_t receiveState = radio.startReceive();
            if (receiveState != RADIOLIB_ERR_NONE)
            {
                log_printf("[LoRa] Restore receive failed, RadioLib error code=%d\n",
                           receiveState);
                Shutdown_LoRa_Test();
            }
            else
            {
                log_printf("[LoRa] Receive listener restored\n");
            }
        }
        else
        {
            if (millis() > CycleTime)
            {
                display.clearDisplay();
                display.setCursor(0, 0);
                display.printf("Lora N");
                display.display();

                log_printf("Lora init fail\n");
                CycleTime = millis() + 1000;
            }
        }

        break;

    default:
        break;
    }

    delay(10);
}
