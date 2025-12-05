#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <hardware/irq.h>
#include <hardware/pio.h>

#include <Adafruit_FT6206.h>
#include "commands.h"
#include "terminal.h"
#include "serial_commands.h"
#include "firmware_version.h"
#include "wiegand_port.h"
#include "wiegand_rx_log.h"
// #include <Adafruit_FT6206.h>  // or FT6236 / GT911 / CST816S  (Touch Controller)

// The onboard LED pin for most RP2040 boards (like Raspberry Pi Pico)
const int LED_PIN = LED_BUILTIN;

// Wiegand RX pins from docs.
constexpr uint8_t PIN_WIEGAND_A_D0 = 2;
constexpr uint8_t PIN_WIEGAND_B_D0 = 6;
constexpr uint8_t PIN_WIEGAND_C_D0 = 12;
constexpr uint8_t PIN_WIEGAND_A_TX_D0 = 4;
constexpr uint8_t PIN_WIEGAND_A_TX_D1 = 1;   // inverted TX lines
constexpr uint8_t PIN_WIEGAND_B_TX_D0 = 9;
constexpr uint8_t PIN_WIEGAND_B_TX_D1 = 8;
constexpr uint8_t PIN_WIEGAND_C_TX_D0 = 11;
constexpr uint8_t PIN_WIEGAND_C_TX_D1 = 14;
constexpr uint8_t PIN_WIEGAND_A_LED = 0;
constexpr uint8_t PIN_WIEGAND_B_LED = 5;
constexpr uint8_t PIN_WIEGAND_C_LED = 15;
constexpr float WIEGAND_RX_CLKDIV = 15.0f;  // 1:150 with system clock
constexpr uint32_t WIEGAND_MESSAGE_QUIET_MS = 5;
constexpr uint8_t PIN_TOUCH_INT = 27;

static WiegandPort g_wiegand_ports[] = {
    WiegandPort(pio0, 0, 0, PIN_WIEGAND_A_D0, 0, PIN_WIEGAND_A_TX_D0, PIN_WIEGAND_A_TX_D1,
                PIN_WIEGAND_A_LED),
    WiegandPort(pio0, 1, 1, PIN_WIEGAND_B_D0, 1, PIN_WIEGAND_B_TX_D0, PIN_WIEGAND_B_TX_D1,
                PIN_WIEGAND_B_LED),
    WiegandPort(pio0, 2, 2, PIN_WIEGAND_C_D0, 2, PIN_WIEGAND_C_TX_D0, PIN_WIEGAND_C_TX_D1,
                PIN_WIEGAND_C_LED),
};

TFT_eSPI tft;
Adafruit_FT6206 touch;
int g_wiegand_offset = -1;
SerialCommandProcessor g_cmd(Serial);

// Command handlers are defined in commands.cpp; register_commands wires them up.

extern "C" void __isr pio0_irq0_handler()
{
    const uint32_t pending = pio0->ints0;
    for (auto &port : g_wiegand_ports)
    {
        const uint32_t src_bit = 1u << (pis_sm0_rx_fifo_not_empty + port.sm_index());
        if (pending & src_bit)
        {
            port.handle_irq();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    //tft.fillRoundRect(1, 1, 200, 50, 10, TFT_BLUE); // Rounded rectangle
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(4);
    tft.println(">1 26b 72/100/110  800/1000/1200");
    tft.println("   0x0123456789012345");
    tft.println("");
    tft.println(">1 26b 72/100/110  800/1000/1200");
    tft.println("   0x0123456789012345");

    TerminalConfig terminalConfig;
    terminalConfig.display = &tft;
    terminalConfig.x = 0;
    terminalConfig.y = 0;
    terminalConfig.width = tft.width();
    terminalConfig.height = tft.height();
    terminalConfig.foreground = TFT_GREEN;
    terminalConfig.background = TFT_BLACK;
    terminalConfig.textSize = 1;
    terminalInit(terminalConfig);
    char banner[48];
    std::snprintf(banner, sizeof(banner), "Wiegand Test v%s", FIRMWARE_VERSION);
    terminalAddLine(banner);

    Wire.setSDA(I2C0_SDA);
    Wire.setSCL(I2C0_SCL);
    Wire.begin();
    pinMode(PIN_TOUCH_INT, INPUT);
    if (touch.begin())
    {
        terminalAddLine("Touch OK");
    }
    else
    {
        terminalAddLine("Touch init failed");
    }

    // Load and start Wiegand A RX PIO: one SM per Wiegand input pin pair.
    g_wiegand_offset = pio_add_program(pio0, &wiegand_rx2_program);
    for (auto &port : g_wiegand_ports)
    {
        port.init(g_wiegand_offset, WIEGAND_RX_CLKDIV);
    }
    const size_t port_count = sizeof(g_wiegand_ports) / sizeof(g_wiegand_ports[0]);
    register_commands(g_cmd, g_wiegand_ports, port_count);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    Serial.println("\r\n\r\n\r\n");
    Serial.println("Wiegand Tester Running....");
}

void loop()
{
    // Serial.println("Hello World");
    // digitalWrite(LED_PIN, HIGH);  // Turn LED on
    // delay(500);                   // Wait half a second
    // digitalWrite(LED_PIN, LOW);   // Turn LED off
    // delay(500);                   // Wait half a second

    // static uint32_t lastLog = 0;
    // const uint32_t now = millis();
    // if (now - lastLog >= 1000) {
    //     lastLog = now;
    //     terminalPrintf("Uptime %lu ms", static_cast<unsigned long>(now));
    // }

    // Pseudocode—API depends on the chosen touch lib:
    // if (touch.touched())
    // {
    //     TS_Point p = touch.getPoint();
    //     terminalSetColor(TFT_WHITE);
    //     terminalPrintf("Touch %d,%d", p.x, p.y);
    //     terminalResetColor();
    // }

    g_cmd.poll();

    for (auto &port : g_wiegand_ports)
    {
        port.process(WIEGAND_MESSAGE_QUIET_MS);
        port.tick();
    }
    delay(5);
}
