#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <hardware/irq.h>
#include <hardware/pio.h>

#include "terminal.h"
#include "serial_commands.h"
#include "wiegand_port.h"
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
constexpr float WIEGAND_RX_CLKDIV = 15.0f;  // 1:150 with system clock
constexpr uint32_t WIEGAND_MESSAGE_QUIET_MS = 5;

static WiegandPort g_wiegand_ports[] = {
    WiegandPort(pio0, 0, 0, PIN_WIEGAND_A_D0, 0, PIN_WIEGAND_A_TX_D0, PIN_WIEGAND_A_TX_D1),
    WiegandPort(pio0, 1, 1, PIN_WIEGAND_B_D0, 1, PIN_WIEGAND_B_TX_D0, PIN_WIEGAND_B_TX_D1),
    WiegandPort(pio0, 2, 2, PIN_WIEGAND_C_D0, 2, PIN_WIEGAND_C_TX_D0, PIN_WIEGAND_C_TX_D1),
};

TFT_eSPI tft;
// Adafruit_FT6206 touch;  // example: FT6206
int g_wiegand_offset = -1;
SerialCommandProcessor g_cmd(Serial);

bool cmd_ping(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    Serial.println("OK");
    return true;
}

int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

bool parse_hex_string(const char *hex, uint8_t *out, size_t out_cap, size_t &out_len)
{
    if (!hex)
    {
        return false;
    }
    const size_t len = std::strlen(hex);
    if (len == 0)
    {
        return false;
    }
    size_t padded_len = len;
    bool prepend_zero = false;
    if (padded_len % 2 != 0)
    {
        padded_len += 1;
        prepend_zero = true;
    }
    const size_t byte_len = padded_len / 2;
    if (byte_len > out_cap)
    {
        return false;
    }
    out_len = byte_len;
    size_t src_index = 0;
    for (size_t i = 0; i < byte_len; ++i)
    {
        int hi = 0;
        int lo = 0;
        if (prepend_zero && src_index == 0)
        {
            hi = 0;
        }
        else
        {
            hi = hex_nibble(hex[src_index++]);
        }
        lo = hex_nibble(hex[src_index++]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool cmd_tx(int argc, char *argv[])
{
    if (argc < 3)
    {
        Serial.println("ERR usage: tx <a|b|c> <hexdata> [bits] [bit_us] [inter_us]");
        return false;
    }

    const char port_char = argv[1][0];
    int port_index = -1;
    if (port_char == 'a')
    {
        port_index = 0;
    }
    else if (port_char == 'b')
    {
        port_index = 1;
    }
    else if (port_char == 'c')
    {
        port_index = 2;
    }
    else
    {
        Serial.println("ERR bad port");
        return false;
    }

    constexpr size_t kMaxTxBytes = 32;
    uint8_t tx_buf[kMaxTxBytes];
    size_t tx_len = 0;
    if (!parse_hex_string(argv[2], tx_buf, kMaxTxBytes, tx_len))
    {
        Serial.println("ERR bad hex");
        return false;
    }

    uint32_t bit_count = 26;
    if (argc >= 4)
    {
        bit_count = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
        if (bit_count == 0)
        {
            Serial.println("ERR bad bits");
            return false;
        }
    }

    uint32_t bit_time_us = 100;
    if (argc >= 5)
    {
        bit_time_us = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10));
    }
    if (bit_time_us == 0)
    {
        bit_time_us = 1; // avoid zero-delay hang
    }

    uint32_t interbit_us = 50;
    if (argc >= 6)
    {
        interbit_us = static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 10));
        if (interbit_us == 0)
        {
            interbit_us = 1; // avoid zero-delay hang
        }
    }

    if (!g_wiegand_ports[port_index].transmit(tx_buf, bit_count, bit_time_us, interbit_us))
    {
        Serial.println("ERR transmit failed");
        return false;
    }

    Serial.println("Done");
    return true;
}

const SerialCommand kCommands[] = {
    {"ping", cmd_ping},
    {"tx", cmd_tx},
};

extern "C" void __isr pio0_irq0_handler()
{
    const uint32_t pending = pio0->ints0;
    for (auto &port : g_wiegand_ports)
    {
        const uint32_t irq_bit = 1u << (pis_interrupt0 + port.irq_index());
        if (pending & irq_bit)
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

    // TerminalConfig terminalConfig;
    // terminalConfig.display = &tft;
    // terminalConfig.x = 0;
    // terminalConfig.y = 40;
    // terminalConfig.width = tft.width();
    // terminalConfig.height = tft.height() - terminalConfig.y;
    // terminalConfig.foreground = TFT_GREEN;
    // terminalConfig.background = TFT_BLACK;
    // terminalConfig.textSize = 1;
    // terminalInit(terminalConfig);
    // terminalAddLine("Terminal ready");

    // Wire.setSDA(I2C0_SDA);
    // Wire.setSCL(I2C0_SCL);
    // Wire.begin();
    // touch.begin();  // varies by library (sometimes needs address or RST/IRQ pins)

    // Load and start Wiegand A RX PIO: one SM per Wiegand input pin pair.
    g_wiegand_offset = pio_add_program(pio0, &wiegand_rx2_program);
    for (auto &port : g_wiegand_ports)
    {
        port.init(g_wiegand_offset, WIEGAND_RX_CLKDIV);
    }
    g_cmd.set_commands(kCommands, sizeof(kCommands) / sizeof(kCommands[0]));
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
    // if (now - lastLog >= 5000) {
    //     lastLog = now;
    //     terminalPrintf("Uptime %lu ms", static_cast<unsigned long>(now));
    // }

    // Pseudocode—API depends on the chosen touch lib:
    // if (touch.touched()) {
    //     int x, y; touch.getPoint(&x, &y);
    //     // Map if needed for rotation/orientation:
    //     tft.fillCircle(x, y, 3, TFT_GREEN);
    // }

    g_cmd.poll();

    for (auto &port : g_wiegand_ports)
    {
        port.process(WIEGAND_MESSAGE_QUIET_MS);
    }
    delay(5);
}
