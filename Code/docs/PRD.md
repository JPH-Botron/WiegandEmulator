Title: RP2350 + ST7796S Display bring-up (Arduino/TFT_eSPI)

Bring up a 480×320 SPI TFT (ST7796S) with capacitive touch (FT6336U) on RP2350 board and render “Hello, World!”.  Cap touch integration will come later.

Scope / Non-Goals

In scope: Arduino framework, TFT_eSPI, SPI1 pinout, screen init, text rendering, timing benchmark.

Out of scope (this phase): capacitive touch integration, LVGL UI, power management.

Hardware

MCU: RP2350 (Pico 2 class)

Display: ST7796S, 480×320, SPI

Pins: SCK=GPIO10, MOSI=GPIO11, MISO=GPIO12 (optional), CS=GPIO13, DC/RS=GPIO14, RST=GPIO15, BL=TBD

Backlight: TBD (GPIO + level or always on)

Software

PlatformIO / Arduino core (Philhower)

Library: TFT_eSPI with compile-time defines (no sprites initially)

Acceptance criteria

Builds and uploads successfully from VS Code/PlatformIO.

“Hello, World!” visible with correct orientation.

Full-screen color fill time reported ≤ ~80 ms @ 40 MHz SPI (or measured + documented).

Code merged with platformio.ini pin/driver defines checked in.

Risks / Unknowns

Board manifest and uploader for RP2350.

Panel variant needing width/height swap or alternate rotation.

Backlight control pin behavior.

Milestones

Wire & init (text on screen)

Measure fill rate (timed fillScreen)

Clean platformio.ini + docs