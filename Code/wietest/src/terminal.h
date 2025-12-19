#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#ifndef TERMINAL_MAX_LINES
#define TERMINAL_MAX_LINES 16
#endif

#ifndef TERMINAL_MAX_CHARS
#define TERMINAL_MAX_CHARS 160
#endif

struct TerminalConfig
{
    TFT_eSPI *display;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t foreground;
    uint16_t background;
    uint8_t textSize;
};

// Initialize the terminal window inside the configured screen region.
void terminalInit(const TerminalConfig &config);

// Append a new line of text to the terminal window.
void terminalAddLine(const char *message);

// Append a new line with a fixed 4-space indent (applied to wrapped lines too).
void terminalAddIndentedLine(const char *message);

// Optional printf-style helper for formatted text.
void terminalPrintf(const char *fmt, ...);

// Set the text color for subsequent lines (foreground only).
void terminalSetColor(uint16_t color);

// Restore default terminal colors.
void terminalResetColor();

// Force a redraw of the terminal contents (e.g., after screen clear).
void terminalRedraw();

// Clear the terminal buffer and the on-screen region.
void terminalClear();

// Enable/disable on-screen rendering; buffer continues to collect lines.
void terminalSetRenderingEnabled(bool enabled);
