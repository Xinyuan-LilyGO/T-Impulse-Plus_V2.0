#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "pin_config.h"

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t SCREEN_I2C_CLOCK_HZ = 100000UL;
static constexpr uint8_t SCREEN_ALT_ADDRESS = 0x3D;

struct I2C_Line_State
{
    int sda = LOW;
    int scl = LOW;
    bool idle = false;
};

static bool Screen_Ready = false;
static uint8_t Screen_Address = SCREEN_ADDRESS;
static uint32_t Next_Heartbeat_Ms = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, SCREEN_RST);

static void Wait_For_Serial(void)
{
    Serial.begin(SERIAL_BAUD);

    const uint32_t started_ms = millis();
    while (!Serial && (millis() - started_ms < 3000UL))
    {
        delay(10);
    }

    Serial.println("[T-Impulse_Plus_v2.0][v2_screen_test] Serial ready");
    Serial.flush();
}

static void Enable_3V3_Rail(void)
{
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
    digitalWrite(RT9080_EN, LOW);
    delay(100);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
}

static void Release_Screen_I2C(void)
{
    Wire1.end();
    pinMode(SCREEN_SDA, INPUT);
    pinMode(SCREEN_SCL, INPUT);
}

static void Print_Pin_Map(void)
{
    Serial.println("[Stage] Screen pin map starting");
    Serial.println("[Pin map] hardware_revision=V2");
    Serial.printf("[Pin map] controller=SSD1315 resolution=%ux%u\r\n",
                  static_cast<unsigned int>(SCREEN_WIDTH),
                  static_cast<unsigned int>(SCREEN_HEIGHT));
    Serial.printf("[Pin map] screen SDA=P1.06 Arduino_pin=%lu\r\n",
                  static_cast<unsigned long>(SCREEN_SDA));
    Serial.printf("[Pin map] screen SCL=P1.04 Arduino_pin=%lu\r\n",
                  static_cast<unsigned long>(SCREEN_SCL));
    Serial.printf("[Pin map] expected_screen_address=0x%02X RESET=none\r\n",
                  static_cast<unsigned int>(SCREEN_ADDRESS));
    Serial.printf("[Safety] TTP223 Q=P0.15 is separate from screen SCL=P1.04; TTP223 is not read\r\n");
    Serial.println("[Stage] Screen pin map complete");
}

static I2C_Line_State Read_Screen_Lines(bool enable_internal_pullups)
{
    pinMode(SCREEN_SDA, enable_internal_pullups ? INPUT_PULLUP : INPUT);
    pinMode(SCREEN_SCL, enable_internal_pullups ? INPUT_PULLUP : INPUT);
    delay(5);

    I2C_Line_State state;
    state.sda = digitalRead(SCREEN_SDA);
    state.scl = digitalRead(SCREEN_SCL);
    state.idle = (state.sda == HIGH) && (state.scl == HIGH);
    return state;
}

static bool Check_Screen_Lines(void)
{
    const I2C_Line_State before_pullups = Read_Screen_Lines(false);
    Serial.printf("[Screen I2C] before_pullups SDA=%d SCL=%d (reference only)\r\n",
                  before_pullups.sda,
                  before_pullups.scl);

    const I2C_Line_State with_pullups = Read_Screen_Lines(true);
    Serial.printf("[Screen I2C] with_pullups SDA=%d SCL=%d idle=%s\r\n",
                  with_pullups.sda,
                  with_pullups.scl,
                  with_pullups.idle ? "yes" : "no");

    if (with_pullups.idle)
    {
        Serial.println("[Screen I2C] line check passed: SDA/SCL are both HIGH");
        return true;
    }

    Serial.println("[Screen I2C] scan disabled while a line is LOW");
    if (with_pullups.sda == LOW)
    {
        Serial.println("[Screen I2C] SDA=P1.06 is held LOW externally; check screen VDD/GND, SDA routing, and soldering first");
    }
    if (with_pullups.scl == LOW)
    {
        Serial.println("[Screen I2C] SCL=P1.04 is held LOW; check screen SCL routing, power, and pull-up resistors");
    }
    Serial.println("[Screen I2C] bus-clock recovery was not attempted; remove the external LOW condition first");
    return false;
}

static uint8_t Probe_Address(uint8_t address)
{
    Wire1.beginTransmission(address);
    const uint8_t error = Wire1.endTransmission();
    Serial.printf("[Screen I2C] probe_address=0x%02X result=%u (0=ACK, 2=address NACK, 4=other error)\r\n",
                  static_cast<unsigned int>(address),
                  static_cast<unsigned int>(error));
    return error;
}

static bool Draw_Screen_Test_Pattern(void)
{
    Serial.println("[Screen] stage=white-screen write");
    display.clearDisplay();
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(300);
    Serial.println("[Screen] white-screen command sent; check whether the panel is fully lit");

    Serial.println("[Screen] stage=test-pattern write");
    display.clearDisplay();
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.drawLine(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.drawLine(SCREEN_WIDTH - 1, 0, 0, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(8, 8);
    display.print("V2 SCREEN TEST");
    display.setCursor(8, 20);
    display.print("SSD1315 I2C OK");
    display.setCursor(8, 32);
    display.printf("ADDR 0x%02X", static_cast<unsigned int>(Screen_Address));
    display.setCursor(8, 44);
    display.print("P1.06/P1.04");
    display.display();

    const I2C_Line_State after_write = Read_Screen_Lines(true);
    Serial.printf("[Screen] test-pattern command sent; after_write SDA=%d SCL=%d idle=%s\r\n",
                  after_write.sda,
                  after_write.scl,
                  after_write.idle ? "yes" : "no");
    return after_write.idle;
}

static bool Initialize_Screen(void)
{
    Serial.println("[Screen I2C] stage=Wire1 start");
    Wire1.setPins(SCREEN_SDA, SCREEN_SCL);
    Wire1.begin();
    Wire1.setClock(SCREEN_I2C_CLOCK_HZ);
    Serial.printf("[Screen I2C] Wire1 started SDA=P1.06 SCL=P1.04 clock=%luHz\r\n",
                  static_cast<unsigned long>(SCREEN_I2C_CLOCK_HZ));

    const uint8_t expected_error = Probe_Address(SCREEN_ADDRESS);
    if (expected_error != 0)
    {
        const uint8_t alternate_error = Probe_Address(SCREEN_ALT_ADDRESS);
        if (alternate_error == 0)
        {
            Screen_Address = SCREEN_ALT_ADDRESS;
            Serial.println("[Screen I2C] found 0x3D instead of configured 0x3C; continuing with 0x3D, check the address jumper later");
        }
        else
        {
            Serial.println("[Screen I2C] neither 0x3C nor 0x3D acknowledged; stopping screen initialization");
            Release_Screen_I2C();
            return false;
        }
    }

    Serial.printf("[Screen] stage=SSD1315 initialization address=0x%02X\r\n",
                  static_cast<unsigned int>(Screen_Address));
    // Wire1 has already been configured with the V2 pins, so do not re-run begin().
    if (!display.begin(SSD1306_SWITCHCAPVCC, Screen_Address, true, false))
    {
        Serial.println("[Screen] SSD1315 initialization failed: framebuffer allocation or controller communication failed");
        Release_Screen_I2C();
        return false;
    }

    Serial.println("[Screen] SSD1315 initialization succeeded");
    return Draw_Screen_Test_Pattern();
}

void setup()
{
    Wait_For_Serial();
    Serial.println("[Stage] Screen diagnostic starting");
    Print_Pin_Map();

    Enable_3V3_Rail();
    Serial.println("[Power] RT9080_EN=P0.19 stabilized with the high-low-high sequence; the screen is powered from the board 3.3V rail");
    Serial.println("[Power] Main I2C, LoRa, GNSS, and TTP223 stay disabled to avoid disturbing the screen test");

    if (!Check_Screen_Lines())
    {
        Serial.println("[Result] Screen test stopped: fix the LOW line condition first; I2C will not be forced on");
        Release_Screen_I2C();
        return;
    }

    Screen_Ready = Initialize_Screen();
    if (Screen_Ready)
    {
        Serial.println("[Result] Screen communication test passed: white screen and test pattern sent");
    }
    else
    {
        Release_Screen_I2C();
        Serial.println("[Result] Screen communication test failed: inspect the address and line diagnostics above");
    }
}

void loop()
{
    if (millis() >= Next_Heartbeat_Ms)
    {
        const I2C_Line_State state = Read_Screen_Lines(true);
        Serial.printf("[Heartbeat] Screen ready=%s SDA=%d SCL=%d idle=%s address=0x%02X\r\n",
                      Screen_Ready ? "yes" : "no",
                      state.sda,
                      state.scl,
                      state.idle ? "yes" : "no",
                      static_cast<unsigned int>(Screen_Address));
        Next_Heartbeat_Ms = millis() + 2000UL;
    }
    delay(10);
}
