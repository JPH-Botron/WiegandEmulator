#pragma once

#include <cstddef>

#include "serial_commands.h"
#include "wiegand_port.h"

// Register all serial commands (ping, ver, help, pins, getrx, tx).
// ports points to an array of WiegandPort instances (port_count entries).
void register_commands(SerialCommandProcessor &processor, WiegandPort *ports, size_t port_count);
