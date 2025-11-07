#include "terminal.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// namespace {

// TerminalConfig g_config{};
// bool g_initialized = false;

// char g_lines[TERMINAL_MAX_LINES][TERMINAL_MAX_CHARS] = {{0}};
// size_t g_nextIndex = 0;
// size_t g_lineCount = 0;

// uint16_t g_lineHeight = 0;

// uint16_t computeLineHeight(const TFT_eSPI& display, uint8_t textSize) {
//   // fontHeight() reflects current settings; default font is 8px tall.
//   int16_t height = display.fontHeight();
//   if (height <= 0) {
//     height = 8;
//   }
//   return static_cast<uint16_t>(height * textSize);
// }

// void render() {
//   if (!g_initialized || g_config.display == nullptr) {
//     return;
//   }

//   TFT_eSPI& tft = *g_config.display;
//   tft.fillRect(g_config.x, g_config.y, g_config.width, g_config.height, g_config.background);
//   tft.setTextColor(g_config.foreground, g_config.background);
//   tft.setTextSize(g_config.textSize);

//   if (g_lineHeight == 0) {
//     g_lineHeight = computeLineHeight(tft, g_config.textSize);
//   }

//   const uint16_t linesVisible = g_lineHeight == 0 ? 0 : g_config.height / g_lineHeight;
//   if (linesVisible == 0 || g_lineCount == 0) {
//     return;
//   }

//   const size_t linesToDraw = g_lineCount < linesVisible ? g_lineCount : linesVisible;
//   const size_t startIndex = (g_nextIndex + TERMINAL_MAX_LINES - linesToDraw) % TERMINAL_MAX_LINES;

//   for (size_t i = 0; i < linesToDraw; ++i) {
//     const size_t bufferIndex = (startIndex + i) % TERMINAL_MAX_LINES;
//     const int16_t cursorY = static_cast<int16_t>(g_config.y + (i * g_lineHeight));
//     tft.setCursor(g_config.x, cursorY);
//     tft.print(g_lines[bufferIndex]);
//   }
// }

// void storeLine(const char* message) {
//   if (!g_initialized) {
//     return;
//   }

//   std::strncpy(g_lines[g_nextIndex], message, TERMINAL_MAX_CHARS - 1);
//   g_lines[g_nextIndex][TERMINAL_MAX_CHARS - 1] = '\0';

//   g_nextIndex = (g_nextIndex + 1U) % TERMINAL_MAX_LINES;
//   if (g_lineCount < TERMINAL_MAX_LINES) {
//     ++g_lineCount;
//   }
// }

// }  // namespace

// void terminalInit(const TerminalConfig& config) {
//   g_config = config;
//   g_initialized = config.display != nullptr;
//   g_nextIndex = 0;
//   g_lineCount = 0;
//   g_lineHeight = 0;
//   std::memset(g_lines, 0, sizeof(g_lines));

//   if (!g_initialized) {
//     return;
//   }

//   g_lineHeight = computeLineHeight(*g_config.display, g_config.textSize);
//   terminalRedraw();
// }

// void terminalAddLine(const char* message) {
//   if (!message) {
//     return;
//   }

//   storeLine(message);
//   render();
// }

// void terminalPrintf(const char* fmt, ...) {
//   if (!fmt) {
//     return;
//   }

//   char buffer[TERMINAL_MAX_CHARS];
//   std::va_list args;
//   va_start(args, fmt);
//   std::vsnprintf(buffer, sizeof(buffer), fmt, args);
//   va_end(args);

//   terminalAddLine(buffer);
// }

// void terminalRedraw() { render(); }

// void terminalClear() {
//   if (!g_initialized || g_config.display == nullptr) {
//     return;
//   }

//   std::memset(g_lines, 0, sizeof(g_lines));
//   g_nextIndex = 0;
//   g_lineCount = 0;

//   g_config.display->fillRect(g_config.x, g_config.y, g_config.width, g_config.height, g_config.background);
// }
