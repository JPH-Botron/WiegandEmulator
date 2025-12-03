#pragma once

#include <Arduino.h>

struct SerialCommand
{
    const char *name;
    bool (*handler)(int argc, char *argv[]);
};

class SerialCommandProcessor
{
public:
    explicit SerialCommandProcessor(Stream &serial);

    // Set the table of supported commands (null handler entries are skipped).
    void set_commands(const SerialCommand *commands, size_t count);

    // Non-blocking pump: call this regularly from loop().
    void poll();

private:
    static constexpr size_t kMaxLineLength = 256;
    static constexpr size_t kMaxArgs = 12;

    void reset_buffer();
    void process_line();
    bool dispatch(char *line);

    Stream &serial_;
    const SerialCommand *commands_;
    size_t command_count_;
    char line_buffer_[kMaxLineLength];
    size_t line_len_;
};
