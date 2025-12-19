#include "display_modes.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "terminal.h"
extern "C" {
#include "qrcodegen.h"
}

namespace {

TFT_eSPI *g_display = nullptr;
DisplayMode g_mode = DisplayMode::Terminal;

constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kFg = TFT_WHITE;
constexpr uint16_t kAccent = TFT_GREEN;

void clear_screen()
{
    if (!g_display) return;
    g_display->fillScreen(kBg);
}

int text_height(TFT_eSPI &tft)
{
    const int h = tft.fontHeight();
    return (h > 0) ? h : 8;
}

void draw_centered_text(const char *text, int y)
{
    if (!g_display || !text) return;
    g_display->setTextColor(kFg, kBg);
    g_display->setTextSize(1);
    const int w = g_display->textWidth(text);
    const int x = (g_display->width() > w) ? (g_display->width() - w) / 2 : 0;
    g_display->setCursor(x, y);
    g_display->print(text);
}

bool render_qr(const char *text)
{
    if (!g_display || !text) return false;
    constexpr size_t kBufSize = qrcodegen_BUFFER_LEN_MAX;
    uint8_t temp[kBufSize];
    uint8_t qr[kBufSize];

    const bool ok = qrcodegen_encodeText(text, temp, qr, qrcodegen_Ecc_LOW,
                                         qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                                         qrcodegen_Mask_AUTO, true);
    if (!ok) return false;

    const int size = qrcodegen_getSize(qr);
    const int quiet = 4;
    const int total = size + quiet * 2;
    const int maxModule = std::min(g_display->width(), g_display->height()) / total;
    if (maxModule <= 0) return false;
    const int module = std::max(1, maxModule);
    const int img_w = total * module;
    const int x0 = (g_display->width() - img_w) / 2;
    const int y0 = (g_display->height() - img_w) / 2;

    clear_screen();
    terminalSetRenderingEnabled(false);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            if (qrcodegen_getModule(qr, x, y))
            {
                g_display->fillRect(x0 + (x + quiet) * module,
                                    y0 + (y + quiet) * module,
                                    module, module, kFg);
            }
        }
    }
    // Show text below if space allows.
    const int line_y = y0 + img_w + 4;
    if (line_y + text_height(*g_display) <= g_display->height())
    {
        draw_centered_text(text, line_y);
    }
    return true;
}

// Code128 (set B) patterns: 107 entries, stop (106) uses 7-width pattern.
// Borrowed from ZXing's CODE_PATTERNS (public domain-like).
static const uint8_t kCode128Patterns[107][7] = {
    {2, 1, 2, 2, 2, 2, 0}, {2, 2, 2, 1, 2, 2, 0}, {2, 2, 2, 2, 2, 1, 0},
    {1, 2, 1, 2, 2, 3, 0}, {1, 2, 1, 3, 2, 2, 0}, {1, 3, 1, 2, 2, 2, 0},
    {1, 2, 2, 2, 1, 3, 0}, {1, 2, 2, 3, 1, 2, 0}, {1, 3, 2, 2, 1, 2, 0},
    {2, 2, 1, 2, 1, 3, 0}, {2, 2, 1, 3, 1, 2, 0}, {2, 3, 1, 2, 1, 2, 0},
    {1, 1, 2, 2, 3, 2, 0}, {1, 2, 2, 1, 3, 2, 0}, {1, 2, 2, 2, 3, 1, 0},
    {1, 1, 3, 2, 2, 2, 0}, {1, 2, 3, 1, 2, 2, 0}, {1, 2, 3, 2, 2, 1, 0},
    {2, 2, 3, 2, 1, 1, 0}, {2, 2, 1, 1, 3, 2, 0}, {2, 2, 1, 2, 3, 1, 0},
    {2, 1, 3, 2, 1, 2, 0}, {2, 2, 3, 1, 1, 2, 0}, {3, 1, 2, 1, 3, 1, 0},
    {3, 1, 1, 2, 2, 2, 0}, {3, 2, 1, 1, 2, 2, 0}, {3, 2, 1, 2, 2, 1, 0},
    {3, 1, 2, 2, 1, 2, 0}, {3, 2, 2, 1, 1, 2, 0}, {3, 2, 2, 2, 1, 1, 0},
    {2, 1, 2, 1, 2, 3, 0}, {2, 1, 2, 3, 2, 1, 0}, {2, 3, 2, 1, 2, 1, 0},
    {1, 1, 1, 3, 2, 3, 0}, {1, 3, 1, 1, 2, 3, 0}, {1, 3, 1, 3, 2, 1, 0},
    {1, 1, 2, 3, 1, 3, 0}, {1, 3, 2, 1, 1, 3, 0}, {1, 3, 2, 3, 1, 1, 0},
    {2, 1, 1, 3, 1, 3, 0}, {2, 3, 1, 1, 1, 3, 0}, {2, 3, 1, 3, 1, 1, 0},
    {1, 1, 2, 1, 3, 3, 0}, {1, 1, 2, 3, 3, 1, 0}, {1, 3, 2, 1, 3, 1, 0},
    {1, 1, 3, 1, 2, 3, 0}, {1, 1, 3, 3, 2, 1, 0}, {1, 3, 3, 1, 2, 1, 0},
    {3, 1, 3, 1, 2, 1, 0}, {2, 1, 1, 3, 3, 1, 0}, {2, 3, 1, 1, 3, 1, 0},
    {2, 1, 3, 1, 1, 3, 0}, {2, 1, 3, 3, 1, 1, 0}, {2, 1, 3, 1, 3, 1, 0},
    {3, 1, 1, 1, 2, 3, 0}, {3, 1, 1, 3, 2, 1, 0}, {3, 3, 1, 1, 2, 1, 0},
    {3, 1, 2, 1, 1, 3, 0}, {3, 1, 2, 3, 1, 1, 0}, {3, 3, 2, 1, 1, 1, 0},
    {3, 1, 4, 1, 1, 1, 0}, {2, 2, 1, 4, 1, 1, 0}, {4, 3, 1, 1, 1, 1, 0},
    {1, 1, 1, 2, 2, 4, 0}, {1, 1, 1, 4, 2, 2, 0}, {1, 2, 1, 1, 2, 4, 0},
    {1, 2, 1, 4, 2, 1, 0}, {1, 4, 1, 1, 2, 2, 0}, {1, 4, 1, 2, 2, 1, 0},
    {1, 1, 2, 2, 1, 4, 0}, {1, 1, 2, 4, 1, 2, 0}, {1, 2, 2, 1, 1, 4, 0},
    {1, 2, 2, 4, 1, 1, 0}, {1, 4, 2, 1, 1, 2, 0}, {1, 4, 2, 2, 1, 1, 0},
    {2, 4, 1, 2, 1, 1, 0}, {2, 2, 1, 1, 1, 4, 0}, {4, 1, 3, 1, 1, 1, 0},
    {2, 4, 1, 1, 1, 2, 0}, {1, 3, 4, 1, 1, 1, 0}, {1, 1, 1, 2, 4, 2, 0},
    {1, 2, 1, 1, 4, 2, 0}, {1, 2, 1, 2, 4, 1, 0}, {1, 1, 4, 2, 1, 2, 0},
    {1, 2, 4, 1, 1, 2, 0}, {1, 2, 4, 2, 1, 1, 0}, {4, 1, 1, 2, 1, 2, 0},
    {4, 2, 1, 1, 1, 2, 0}, {4, 2, 1, 2, 1, 1, 0}, {2, 1, 2, 1, 4, 1, 0},
    {2, 1, 4, 1, 2, 1, 0}, {4, 1, 2, 1, 2, 1, 0}, {1, 1, 1, 1, 4, 3, 0},
    {1, 1, 1, 3, 4, 1, 0}, {1, 3, 1, 1, 4, 1, 0}, {1, 1, 4, 1, 1, 3, 0},
    {1, 1, 4, 3, 1, 1, 0}, {4, 1, 1, 1, 1, 3, 0}, {4, 1, 1, 3, 1, 1, 0},
    {1, 1, 3, 1, 4, 1, 0}, {1, 1, 4, 1, 3, 1, 0}, {3, 1, 1, 1, 4, 1, 0},
    {4, 1, 1, 1, 3, 1, 0}, {2, 1, 1, 4, 1, 2, 0}, {2, 1, 1, 2, 1, 4, 0},
    {2, 1, 1, 2, 3, 2, 0}, {2, 3, 3, 1, 1, 1, 2} // stop (106) uses 7 entries
};

bool encode_code128_b(const char *text, uint8_t *codes, size_t &code_count, size_t max_codes)
{
    if (!text || !codes) return false;
    const size_t len = std::strlen(text);
    if (len == 0 || len + 3 > max_codes) return false; // start + checksum + stop

    // Validate printable ASCII for set B.
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 32 || c > 126)
        {
            return false;
        }
    }

    size_t idx = 0;
    codes[idx++] = 104; // Start Code B
    int checksum = 104;
    for (size_t i = 0; i < len; ++i)
    {
        const uint8_t code = static_cast<uint8_t>(text[i] - 32);
        codes[idx++] = code;
        checksum = (checksum + static_cast<int>(code) * static_cast<int>(i + 1)) % 103;
    }
    codes[idx++] = static_cast<uint8_t>(checksum);
    codes[idx++] = 106; // stop
    code_count = idx;
    return true;
}

bool render_barcode(const char *text)
{
    if (!g_display || !text) return false;
    constexpr size_t kMaxCodes = 100;
    uint8_t codes[kMaxCodes];
    size_t code_count = 0;
    if (!encode_code128_b(text, codes, code_count, kMaxCodes))
    {
        return false;
    }

    // Flatten codes into module run lengths.
    constexpr int kQuietModules = 10;
    int module_count = kQuietModules * 2; // quiet zones
    for (size_t i = 0; i < code_count; ++i)
    {
        const uint8_t *pattern = kCode128Patterns[codes[i]];
        const int widths = (codes[i] == 106) ? 7 : 6;
        for (int k = 0; k < widths; ++k)
        {
            module_count += pattern[k];
        }
    }

    const int module = std::max(1, g_display->width() / module_count);
    if (module == 0) return false;
    const int barcode_w = module_count * module;
    if (barcode_w > g_display->width()) return false;

    clear_screen();
    terminalSetRenderingEnabled(false);
    const int text_h = text_height(*g_display);
    const int bar_h = g_display->height() - text_h - 6;
    const int y0 = 4;
    int x = (g_display->width() - barcode_w) / 2;

    // Left quiet zone.
    x += kQuietModules * module;

    bool draw_bar = true;
    for (size_t i = 0; i < code_count; ++i)
    {
        const uint8_t *pattern = kCode128Patterns[codes[i]];
        const int widths = (codes[i] == 106) ? 7 : 6;
        for (int k = 0; k < widths; ++k)
        {
            const int w = pattern[k] * module;
            if (draw_bar)
            {
                g_display->fillRect(x, y0, w, bar_h, kFg);
            }
            x += w;
            draw_bar = !draw_bar;
        }
    }

    // Right quiet zone already implied by margin.
    draw_centered_text(text, y0 + bar_h + 2);
    return true;
}

} // namespace

void display_modes_init(TFT_eSPI *display)
{
    g_display = display;
    g_mode = DisplayMode::Terminal;
}

DisplayMode display_current_mode()
{
    return g_mode;
}

bool display_show_qrcode(const char *text)
{
    if (!render_qr(text))
    {
        return false;
    }
    g_mode = DisplayMode::QRCode;
    return true;
}

bool display_show_barcode128(const char *text)
{
    if (!render_barcode(text))
    {
        return false;
    }
    g_mode = DisplayMode::Barcode128;
    return true;
}

void display_show_terminal()
{
    if (!g_display) return;
    clear_screen();
    terminalSetRenderingEnabled(true);
    terminalRedraw();
    g_mode = DisplayMode::Terminal;
}

void display_clear_black()
{
    clear_screen();
    terminalSetRenderingEnabled(false);
    g_mode = DisplayMode::Terminal;
}
