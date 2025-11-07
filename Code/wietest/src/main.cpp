#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cstdio>

#include "terminal.h"
// #include <Adafruit_FT6206.h>  // or FT6236 / GT911 / CST816S  (Touch Controller)

// The onboard LED pin for most RP2040 boards (like Raspberry Pi Pico)
const int LED_PIN = LED_BUILTIN;

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
}

void loop() {
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
}
