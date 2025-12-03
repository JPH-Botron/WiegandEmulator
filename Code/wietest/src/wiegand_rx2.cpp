#include "wiegand_rx2.pio.h"
#include <hardware/pio.h>

// Use the pioasm-generated program (wiegand_rx_program) and default config helper.
// This file only provides a thin init wrapper that sets pins and clock divider.

void wiegand_rx2_program_init(PIO pio, uint sm, uint offset, uint pin, float clk_div)
{
    pio_sm_config c = wiegand_rx2_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);  // reads the configured GPIO as bit0
    sm_config_set_clkdiv(&c, clk_div);

    // Ensure pins are inputs (Wiegand lines are externally driven/open-drain).
    pio_gpio_init(pio, pin);
    pio_gpio_init(pio, pin + 1);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 2, false);
    sm_config_set_in_shift(&c,
                           /* shift_right = */ false,  // push towards MSB, new bits at LSB
                           /* autopush    = */ false,
                           /* push_thresh = */ 32);
    pio_sm_init(pio, sm, offset, &c);
}
