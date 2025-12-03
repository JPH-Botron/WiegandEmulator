#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cstdio>
#include <hardware/pio.h>

#include "terminal.h"
#include "wiegand_rx2.h"
// #include <Adafruit_FT6206.h>  // or FT6236 / GT911 / CST816S  (Touch Controller)

// The onboard LED pin for most RP2040 boards (like Raspberry Pi Pico)
const int LED_PIN = LED_BUILTIN;

// Wiegand A RX pins from docs (GPIO2 = D0, GPIO3 = D1 (GPIO3/D1 are implied)).
constexpr uint8_t PIN_WIEGAND_A_D0 = 2;
constexpr float WIEGAND_RX_CLKDIV = 15.0f;  // 1:150 with system clock

PIO g_pio = pio0;
uint g_wiegand_sm_a = 0;  // SM for Wiegand A
int g_wiegand_offset = -1;

TFT_eSPI tft;
// Adafruit_FT6206 touch;  // example: FT6206

void setup() {
  Serial.begin(115200);
  Serial.println("Hello World");
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
  g_wiegand_offset = pio_add_program(g_pio, &wiegand_rx2_program);
  wiegand_rx2_program_init(g_pio, g_wiegand_sm_a, g_wiegand_offset, PIN_WIEGAND_A_D0, WIEGAND_RX_CLKDIV);
  pio_sm_set_enabled(g_pio, g_wiegand_sm_a, true);
  pio_interrupt_clear(g_pio, 0);
  Serial.println("Wiegand A RX PIO armed (D0/D1)");
}

void loop() {
  uint32_t lastts = 0x3fffffff;
  Serial.println("Hello World");
  digitalWrite(LED_PIN, HIGH);  // Turn LED on
  delay(500);                   // Wait half a second
  digitalWrite(LED_PIN, LOW);   // Turn LED off
  delay(500);                   // Wait half a second

  // static uint32_t lastLog = 0;
  // const uint32_t now = millis();
  // if (now - lastLog >= 5000) {
  //   lastLog = now;
  //   terminalPrintf("Uptime %lu ms", static_cast<unsigned long>(now));
  // }

  // Pseudocode—API depends on the chosen touch lib:
  // if (touch.touched()) {
  //   int x, y; touch.getPoint(&x, &y);
  //   // Map if needed for rotation/orientation:
  //   tft.fillCircle(x, y, 3, TFT_GREEN);
  // }

  // Drain any captured Wiegand A edges to avoid stalling the PIO FIFO.
  while (!pio_sm_is_rx_fifo_empty(g_pio, g_wiegand_sm_a)) {
    const uint32_t word = pio_sm_get(g_pio, g_wiegand_sm_a);
    const uint32_t levels = word & 0x3u;
    const uint32_t ts = word >> 2;  // down-counter snapshot from PIO
    Serial.print("Wiegand A edges=");
    Serial.print(levels);
    Serial.print(" ts=");
    Serial.println(lastts-ts);
    lastts = ts;
  }


  // Clear shared PIO interrupt after draining both FIFOs.
  if (pio_interrupt_get(g_pio, 0)) {
    pio_interrupt_clear(g_pio, 0);
  }
}
