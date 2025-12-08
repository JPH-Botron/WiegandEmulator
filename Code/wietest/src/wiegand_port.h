#pragma once

#include <Arduino.h>
#include <hardware/pio.h>
#include <pico/time.h>

#include "wiegand_rx2.h"

class WiegandPort
{
public:
    WiegandPort(PIO pio, uint sm, uint irq_index, uint pin_base_d0, uint port_id, uint tx_pin_d0,
                uint tx_pin_d1, uint led_pin);

    void init(uint program_offset, float clk_div);
    void handle_irq();
    void reset_buffer();
    uint32_t buffer_level() const;
    bool message_ready(uint32_t quiet_ms) const;
    bool process(uint32_t quiet_ms);
    void tick();
    bool transmit(const uint8_t *data, size_t data_bytes, uint32_t bit_count, uint32_t bit_time_us,
                  uint32_t interbit_time_us);

    uint irq_index() const
    {
        return irq_index_;
    }

    uint sm_index() const
    {
        return sm_;
    }

    uint rx_pin_d0() const
    {
        return pin_base_d0_;
    }

    uint rx_pin_d1() const
    {
        return pin_base_d0_ + 1;
    }

private:
    // Edge records per message: two edges per bit (fall+rise). Support up to 512 bits.
    static constexpr uint32_t kBufferCapacity = 1024;
    static constexpr uint32_t kTxBufferBytes = 32;  // 256 bits max
    static constexpr uint32_t kMaxBits = kTxBufferBytes * 8;

    enum class TxState { Idle, Pulse, InterBit };

    static bool tx_timer_trampoline(repeating_timer_t *rt);
    bool handle_tx_timer();
    void drive_idle();
    void drive_bit(bool bit_is_one);
    void trigger_led(uint32_t duration_ms = 500);

    PIO pio_;
    uint sm_;
    uint irq_index_;
    uint pin_base_d0_;
    uint port_id_;
    uint tx_pin_d0_;
    uint tx_pin_d1_;
    uint led_pin_;
    volatile uint32_t buffer_[kBufferCapacity];
    volatile uint32_t count_;
    volatile uint32_t last_transition_ms_;

    // Transmit state
    repeating_timer_t tx_timer_;
    volatile bool tx_active_;
    TxState tx_state_;
    uint32_t tx_bits_;
    uint32_t tx_bytes_;
    uint32_t tx_bit_index_;
    uint32_t tx_bit_time_us_;
    uint32_t tx_interbit_time_us_;
    uint8_t tx_buffer_[kTxBufferBytes];
    uint32_t led_off_deadline_ms_;
};
