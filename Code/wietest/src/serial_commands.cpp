#include "serial_commands.h"

#include <cstring>
#include <cctype>

SerialCommandProcessor::SerialCommandProcessor(Stream &serial)
    : serial_(serial),
      commands_(nullptr),
      command_count_(0),
      line_buffer_{},
      line_len_(0),
      last_line_{},
      have_last_(false)
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

        // Single '=' immediately triggers replay without requiring newline.
        if (ch == '=' && line_len_ == 0)
        {
            line_buffer_[line_len_++] = '=';
            line_buffer_[line_len_] = '\0';
            process_line();
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
    if (line_buffer_[0] == '=' && have_last_)
    {
        replay_last();
    }
    else
    {
        remember_last(line_buffer_);
        dispatch(line_buffer_);
    }
}

bool SerialCommandProcessor::dispatch(char *line)
{
    if (!line || !commands_ || command_count_ == 0)
    {
        return false;
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

    // Lowercase only the command token for comparison; leave args untouched.
    char cmd_lower[kMaxLineLength];
    std::strncpy(cmd_lower, argv[0], sizeof(cmd_lower) - 1);
    cmd_lower[sizeof(cmd_lower) - 1] = '\0';
    for (char *p = cmd_lower; *p; ++p)
    {
        *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    }

    for (size_t i = 0; i < command_count_; ++i)
    {
        const SerialCommand &cmd = commands_[i];
        if (!cmd.name || !cmd.handler)
        {
            continue;
        }
        if (std::strcmp(cmd.name, cmd_lower) == 0)
        {
            return cmd.handler(argc, argv);
        }
    }

    serial_.println("ERR unknown command");
    return false;
}

void SerialCommandProcessor::remember_last(const char *line)
{
    if (!line)
    {
        have_last_ = false;
        last_line_[0] = '\0';
        return;
    }
    std::strncpy(last_line_, line, sizeof(last_line_) - 1);
    last_line_[sizeof(last_line_) - 1] = '\0';
    have_last_ = true;
}

bool SerialCommandProcessor::replay_last()
{
    if (!have_last_)
    {
        return false;
    }
    // Copy last_line_ into a temp buffer for tokenization.
    char temp[kMaxLineLength];
    std::strncpy(temp, last_line_, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    return dispatch(temp);
}
