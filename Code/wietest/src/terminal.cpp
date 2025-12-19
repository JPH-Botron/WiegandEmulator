#include "terminal.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

TerminalConfig g_config{};
bool g_initialized = false;

char g_lines[TERMINAL_MAX_LINES][TERMINAL_MAX_CHARS] = {{0}};
uint16_t g_colors[TERMINAL_MAX_LINES] = {0};
size_t g_nextIndex = 0;
size_t g_lineCount = 0;

uint16_t g_lineHeight = 0;
uint16_t g_currentColor = TFT_GREEN;
bool g_render_enabled = true;

uint16_t computeLineHeight(TFT_eSPI &display, uint8_t textSize)
{
    // fontHeight() reflects current settings; default font is 8px tall.
    int16_t height = display.fontHeight();
    if (height <= 0)
    {
        height = 8;
    }
    return static_cast<uint16_t>(height * textSize);
}

uint16_t computeCharWidth(TFT_eSPI &display, uint8_t textSize)
{
    display.setTextSize(textSize);
    int16_t width = display.textWidth("W"); // representative glyph
    if (width <= 0)
    {
        width = static_cast<int16_t>(6 * textSize); // fallback width for classic font
    }
    return static_cast<uint16_t>(width);
}

void render()
{
    if (!g_initialized || g_config.display == nullptr || !g_render_enabled)
    {
        return;
    }

    TFT_eSPI &tft = *g_config.display;
    tft.fillRect(g_config.x, g_config.y, g_config.width, g_config.height, g_config.background);
    tft.setTextColor(g_currentColor, g_config.background);
    tft.setTextSize(g_config.textSize);

    if (g_lineHeight == 0)
    {
        g_lineHeight = computeLineHeight(tft, g_config.textSize);
    }

    const uint16_t linesVisible = g_lineHeight == 0 ? 0 : g_config.height / g_lineHeight;
    if (linesVisible == 0 || g_lineCount == 0)
    {
        return;
    }

    const size_t linesToDraw = g_lineCount < linesVisible ? g_lineCount : linesVisible;
    const size_t startIndex = (g_nextIndex + TERMINAL_MAX_LINES - linesToDraw) % TERMINAL_MAX_LINES;

    for (size_t i = 0; i < linesToDraw; ++i)
    {
        const size_t bufferIndex = (startIndex + i) % TERMINAL_MAX_LINES;
        const int16_t cursorY = static_cast<int16_t>(g_config.y + (i * g_lineHeight));
        const uint16_t color = g_colors[bufferIndex] == 0 ? g_config.foreground : g_colors[bufferIndex];
        tft.setTextColor(color, g_config.background);
        tft.setCursor(g_config.x, cursorY);
        tft.print(g_lines[bufferIndex]);
    }
}

void storeLine(const char *message)
{
    if (!g_initialized)
    {
        return;
    }

    std::strncpy(g_lines[g_nextIndex], message, TERMINAL_MAX_CHARS - 1);
    g_lines[g_nextIndex][TERMINAL_MAX_CHARS - 1] = '\0';
    g_colors[g_nextIndex] = g_currentColor;

    g_nextIndex = (g_nextIndex + 1U) % TERMINAL_MAX_LINES;
    if (g_lineCount < TERMINAL_MAX_LINES)
    {
        ++g_lineCount;
    }
}

}  // namespace

void terminalInit(const TerminalConfig &config)
{
    g_config = config;
    g_initialized = config.display != nullptr;
    g_nextIndex = 0;
    g_lineCount = 0;
    g_lineHeight = 0;
    g_currentColor = config.foreground;
    g_render_enabled = true;
    std::memset(g_lines, 0, sizeof(g_lines));
    std::memset(g_colors, 0, sizeof(g_colors));

    if (!g_initialized)
    {
        return;
    }

    g_lineHeight = computeLineHeight(*g_config.display, g_config.textSize);
    terminalRedraw();
}

void terminalAddLine(const char *message)
{
    if (!message)
    {
        return;
    }

    if (!g_initialized || g_config.display == nullptr)
    {
        return;
    }

    g_config.display->setTextSize(g_config.textSize);
    const size_t msgLen = std::strlen(message);
    size_t offset = 0;
    char lineBuf[TERMINAL_MAX_CHARS];
    const char *indent = "    ";
    const size_t indentLen = 0; // default: no indent

    while (offset < msgLen)
    {
        size_t chunk = 0;
        const size_t remaining = msgLen - offset;
        const size_t maxCopy = remaining < (sizeof(lineBuf) - 1) ? remaining : (sizeof(lineBuf) - 1);
        // Grow chunk until it no longer fits the terminal width.
        for (size_t len = 1; len <= maxCopy; ++len)
        {
            size_t totalLen = len + indentLen;
            if (totalLen >= sizeof(lineBuf))
            {
                break;
            }
            if (indentLen > 0)
            {
                std::memcpy(lineBuf, indent, indentLen);
                std::memcpy(lineBuf + indentLen, message + offset, len);
                lineBuf[indentLen + len] = '\0';
            }
            else
            {
                std::memcpy(lineBuf, message + offset, len);
                lineBuf[len] = '\0';
            }

            if (g_config.display->textWidth(lineBuf) <= g_config.width)
            {
                chunk = len;
            }
            else
            {
                break;
            }
        }
        if (chunk == 0)
        {
            chunk = 1; // always make forward progress
            if (indentLen > 0)
            {
                std::memcpy(lineBuf, indent, indentLen);
                lineBuf[indentLen] = message[offset];
                lineBuf[indentLen + 1] = '\0';
            }
            else
            {
                lineBuf[0] = message[offset];
                lineBuf[1] = '\0';
            }
        }
        storeLine(lineBuf);
        offset += chunk;
    }

    render();
}

void terminalAddIndentedLine(const char *message)
{
    if (!message)
    {
        return;
    }

    if (!g_initialized || g_config.display == nullptr)
    {
        return;
    }

    g_config.display->setTextSize(g_config.textSize);
    static const char indent[] = "    "; // 4 spaces
    const size_t indentLen = sizeof(indent) - 1;
    const size_t msgLen = std::strlen(message);
    size_t offset = 0;
    char lineBuf[TERMINAL_MAX_CHARS];

    while (offset < msgLen)
    {
        size_t chunk = 0;
        const size_t remaining = msgLen - offset;
        const size_t maxCopy = remaining < (sizeof(lineBuf) - 1) ? remaining : (sizeof(lineBuf) - 1);
        for (size_t len = 1; len <= maxCopy; ++len)
        {
            const size_t totalLen = len + indentLen;
            if (totalLen >= sizeof(lineBuf))
            {
                break;
            }
            std::memcpy(lineBuf, indent, indentLen);
            std::memcpy(lineBuf + indentLen, message + offset, len);
            lineBuf[indentLen + len] = '\0';
            if (g_config.display->textWidth(lineBuf) <= g_config.width)
            {
                chunk = len;
            }
            else
            {
                break;
            }
        }
        if (chunk == 0)
        {
            chunk = 1;
            std::memcpy(lineBuf, indent, indentLen);
            lineBuf[indentLen] = message[offset];
            lineBuf[indentLen + 1] = '\0';
        }
        storeLine(lineBuf);
        offset += chunk;
    }

    render();
}

void terminalSetColor(uint16_t color)
{
    g_currentColor = color;
}

void terminalResetColor()
{
    g_currentColor = g_config.foreground;
}

void terminalPrintf(const char *fmt, ...)
{
    if (!fmt)
    {
        return;
    }

    char buffer[TERMINAL_MAX_CHARS];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    terminalAddLine(buffer);
}

void terminalRedraw()
{
    render();
}

void terminalClear()
{
    if (!g_initialized || g_config.display == nullptr || !g_render_enabled)
    {
        return;
    }

    std::memset(g_lines, 0, sizeof(g_lines));
    g_nextIndex = 0;
    g_lineCount = 0;

    g_config.display->fillRect(g_config.x, g_config.y, g_config.width, g_config.height,
                               g_config.background);
}

void terminalSetRenderingEnabled(bool enabled)
{
    g_render_enabled = enabled;
    if (enabled)
    {
        render();
    }
}
