#pragma once

#include "wiegand_rx2pio.h"

// Helper to configure the state machine for Wiegand RX on a single pin.
// pin is the GPIO to sample; clk_div sets the SM clock divider.
void wiegand_rx2_program_init(PIO pio, uint sm, uint offset, uint pin, float clk_div);

