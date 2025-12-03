#include "serial_commands.h"

#include <cstring>
#include <cctype>

SerialCommandProcessor::SerialCommandProcessor(Stream &serial)
    : serial_(serial),
      commands_(nullptr),
      command_count_(0),
      line_buffer_{},
      line_len_(0)
{
}

void SerialCommandProcessor::set_commands(const SerialCommand *commands, size_t count)
{
    commands_ = commands;
    command_count_ = count;
}

void SerialCommandProcessor::reset_buffer()
{
    line_len_ = 0;
    line_buffer_[0] = '\0';
}

void SerialCommandProcessor::poll()
{
    while (serial_.available() > 0)
    {
        const int ch = serial_.read();
        if (ch < 0)
        {
            break;
        }

        // Handle backspace/delete.
        if (ch == '\b' || ch == 0x7f)
        {
            if (line_len_ > 0)
            {
                line_len_--;
                line_buffer_[line_len_] = '\0';
            }
            continue;
        }

        // Treat CR or LF as end-of-line.
        if (ch == '\r' || ch == '\n')
        {
            if (line_len_ > 0)
            {
                line_buffer_[line_len_] = '\0';
                process_line();
            }
            reset_buffer();
            continue;
        }

        // Append character if space remains.
        if (line_len_ < kMaxLineLength - 1)
        {
            line_buffer_[line_len_++] = static_cast<char>(ch);
        }
        else
        {
            // Line too long: drop and reset.
            reset_buffer();
        }
    }
}

void SerialCommandProcessor::process_line()
{
    dispatch(line_buffer_);
}

bool SerialCommandProcessor::dispatch(char *line)
{
    if (!line || !commands_ || command_count_ == 0)
    {
        return false;
    }

    // Lowercase the line in place to make command/args case-insensitive.
    for (char *p = line; *p; ++p)
    {
        *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    }

    // Tokenize in place (command + args).
    char *argv[kMaxArgs];
    int argc = 0;
    char *token = std::strtok(line, " ");
    while (token && argc < static_cast<int>(kMaxArgs))
    {
        argv[argc++] = token;
        token = std::strtok(nullptr, " ");
    }
    if (argc == 0)
    {
        return false;
    }

    for (size_t i = 0; i < command_count_; ++i)
    {
        const SerialCommand &cmd = commands_[i];
        if (!cmd.name || !cmd.handler)
        {
            continue;
        }
        if (std::strcmp(cmd.name, argv[0]) == 0)
        {
            return cmd.handler(argc, argv);
        }
    }

    serial_.println("ERR unknown command");
    return false;
}
