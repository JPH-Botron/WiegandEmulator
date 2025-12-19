#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class DisplayMode
{
    Terminal,
    QRCode,
    Barcode128,
};

// Must be called once after the TFT is initialized.
void display_modes_init(TFT_eSPI *display);

// Current screen state (purely local; terminal state is preserved off-screen).
DisplayMode display_current_mode();

// Render a QR code for the given text. Returns false on invalid input or render failure.
bool display_show_qrcode(const char *text);

// Render a Code128 (set B) barcode with human-readable text beneath. Returns false on failure.
bool display_show_barcode128(const char *text);

// Switch back to the scrolling terminal view and redraw its buffered lines.
void display_show_terminal();

// Clear the LCD to black without drawing any view; leaves terminal rendering disabled.
void display_clear_black();
