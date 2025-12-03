#include "wiegand_port.h"

#include <cstring>
#include <climits>
#include <hardware/gpio.h>

WiegandPort::WiegandPort(PIO pio, uint sm, uint irq_index, uint pin_base_d0, uint port_id,
                         uint tx_pin_d0, uint tx_pin_d1)
    : pio_(pio),
      sm_(sm),
      irq_index_(irq_index),
      pin_base_d0_(pin_base_d0),
      port_id_(port_id),
      tx_pin_d0_(tx_pin_d0),
      tx_pin_d1_(tx_pin_d1),
      buffer_{},
      count_(0),
      last_transition_ms_(0),
      tx_timer_{},
      tx_active_(false),
      tx_state_(TxState::Idle),
      tx_bits_(0),
      tx_bit_index_(0),
      tx_bit_time_us_(0),
      tx_interbit_time_us_(0),
      tx_buffer_{} {}

void WiegandPort::init(uint program_offset, float clk_div)
{
    wiegand_rx2_program_init(pio_, sm_, program_offset, pin_base_d0_, clk_div);
    pio_interrupt_clear(pio_, irq_index_);
    pio_set_irq0_source_enabled(
        pio_, static_cast<pio_interrupt_source_t>(pis_interrupt0 + irq_index_), true);
    pio_sm_set_enabled(pio_, sm_, true);

    pinMode(tx_pin_d0_, OUTPUT);
    pinMode(tx_pin_d1_, OUTPUT);
    drive_idle();
}

void WiegandPort::handle_irq()
{
    while (!pio_sm_is_rx_fifo_empty(pio_, sm_))
    {
        const uint32_t word = pio_sm_get(pio_, sm_);
        last_transition_ms_ = millis();
        if (count_ < kBufferCapacity)
        {
            buffer_[count_++] = word;
        }
        // Otherwise drop and keep draining to prevent stalls.
    }
    pio_interrupt_clear(pio_, irq_index_);
}

void WiegandPort::reset_buffer()
{
    noInterrupts();
    count_ = 0;
    interrupts();
}

uint32_t WiegandPort::buffer_level() const
{
    uint32_t count_snapshot;
    noInterrupts();
    count_snapshot = count_;
    interrupts();
    return count_snapshot;
}

bool WiegandPort::message_ready(uint32_t quiet_ms) const
{
    uint32_t count_snapshot;
    uint32_t last_ms_snapshot;
    noInterrupts();
    count_snapshot = count_;
    last_ms_snapshot = last_transition_ms_;
    interrupts();
    if (count_snapshot < 2)
    {
        return false;
    }
    const uint32_t now = millis();
    return (now - last_ms_snapshot) >= quiet_ms;
}

bool WiegandPort::process(uint32_t quiet_ms)
{
    if (!message_ready(quiet_ms))
    {
        return false;
    }

    // Snapshot buffer under interrupt lock, then print.
    uint32_t local_count = 0;
    uint32_t local_buffer[kBufferCapacity];
    noInterrupts();
    local_count = count_;
    for (uint32_t i = 0; i < local_count; ++i)
    {
        local_buffer[i] = buffer_[i];
    }
    interrupts();

    Serial.print("Message complete on port ");
    Serial.println(port_id_);
    uint32_t prev_ts = 0;
    uint32_t prev_levels = 0x3; // assume idle high on both lines
    uint32_t last_fall_ts[2] = {0, 0};
    bool in_low[2] = {false, false};
    uint32_t min_low[2] = {UINT32_MAX, UINT32_MAX};
    uint32_t max_low[2] = {0, 0};
    uint64_t sum_low[2] = {0, 0};
    uint32_t count_low[2] = {0, 0};
    const uint32_t wrap = (1u << 30);
    uint8_t bits[64] = {0}; // up to 512 bits
    uint32_t bit_count = 0;

    for (uint32_t i = 0; i < local_count; ++i)
    {
        const uint32_t word = local_buffer[i];
        const uint32_t ts = word >> 2;
        const uint32_t levels = word & 0x3;
        const uint32_t d0 = levels & 0x1;
        const uint32_t d1 = (levels >> 1) & 0x1;
        Serial.print(ts);
        if (i == 0)
        {
            Serial.print(" +0");
        }
        else
        {
            // Down-counter delta: previous minus current, with wrap on 30 bits.
            const uint32_t delta = (prev_ts >= ts) ? (prev_ts - ts) : (prev_ts + wrap - ts);
            Serial.print(" +");
            Serial.print(delta);
        }
        Serial.print(" ");
        Serial.print(d0);
        Serial.print(" ");
        Serial.println(d1);
        prev_ts = ts;

        // Track low pulse durations per line.
        for (int line = 0; line < 2; ++line)
        {
            const uint32_t mask = 1u << line;
            const bool was_high = (prev_levels & mask) != 0;
            const bool now_high = (levels & mask) != 0;
            if (was_high && !now_high)
            {
                // Falling edge: start of low pulse.
                last_fall_ts[line] = ts;
                in_low[line] = true;
            }
            else if (!was_high && now_high && in_low[line])
            {
                // Rising edge: end of low pulse.
                const uint32_t pulse = (last_fall_ts[line] >= ts)
                                           ? (last_fall_ts[line] - ts)
                                           : (last_fall_ts[line] + wrap - ts);
                if (pulse < min_low[line])
                {
                    min_low[line] = pulse;
                }
                if (pulse > max_low[line])
                {
                    max_low[line] = pulse;
                }
                sum_low[line] += pulse;
                count_low[line] += 1;
                in_low[line] = false;

                // Record bit: D0 pulse -> 0, D1 pulse -> 1
                if (bit_count < 512)
                {
                    if (line == 1)
                    {
                        bits[bit_count >> 3] |= static_cast<uint8_t>(1u << (bit_count & 7));
                    }
                    bit_count++;
                }
            }
        }
        prev_levels = levels;
    }

    for (int line = 0; line < 2; ++line)
    {
        const char *name = (line == 0) ? "D0" : "D1";
        if (count_low[line] == 0)
        {
            Serial.print(name);
            Serial.println(" low: no pulses");
            continue;
        }
        const uint32_t avg = static_cast<uint32_t>(sum_low[line] / count_low[line]);
        Serial.print(name);
        Serial.print(" low min/max/avg: ");
        Serial.print(min_low[line]);
        Serial.print("/");
        Serial.print(max_low[line]);
        Serial.print("/");
        Serial.println(avg);
    }

    // Emit captured bits in hex and bit count.
    Serial.print("Bits (hex): ");
    const uint32_t byte_len = (bit_count + 7) / 8;
    auto hexchar = [](uint8_t nib) -> char
    {
        return (nib < 10) ? static_cast<char>('0' + nib) : static_cast<char>('a' + (nib - 10));
    };
    for (uint32_t i = 0; i < byte_len; ++i)
    {
        const uint8_t b = bits[i];
        Serial.print(hexchar((b >> 4) & 0xF));
        Serial.print(hexchar(b & 0xF));
    }
    Serial.print(" bits=");
    Serial.println(bit_count);

    reset_buffer();
    return true;
}

bool WiegandPort::tx_timer_trampoline(repeating_timer_t *rt)
{
    return static_cast<WiegandPort *>(rt->user_data)->handle_tx_timer();
}

bool WiegandPort::handle_tx_timer()
{
    // Runs in IRQ context.
    if (!tx_active_)
    {
        return false;
    }

    switch (tx_state_)
    {
    case TxState::Pulse:
    {
        drive_idle(); // end of pulse; release lines
        tx_bit_index_++;
        if (tx_bit_index_ >= tx_bits_)
        {
            tx_active_ = false;
            tx_state_ = TxState::Idle;
            return false; // stop timer
        }
        tx_state_ = TxState::InterBit;
        tx_timer_.delay_us = tx_interbit_time_us_;
        return true;
    }
    case TxState::InterBit:
    {
        const uint32_t byte_idx = tx_bit_index_ >> 3;
        const uint32_t bit_idx = tx_bit_index_ & 7;
        const uint8_t byte = tx_buffer_[byte_idx];
        const bool bit_is_one = (byte >> bit_idx) & 0x1;
        drive_bit(bit_is_one);
        tx_state_ = TxState::Pulse;
        tx_timer_.delay_us = tx_bit_time_us_;
        return true;
    }
    case TxState::Idle:
    default:
        tx_active_ = false;
        return false;
    }
}

void WiegandPort::drive_idle()
{
    // Inverted outputs: drive low (GPIO low) to deassert.
    gpio_put(tx_pin_d0_, 0);
    gpio_put(tx_pin_d1_, 0);
}

void WiegandPort::drive_bit(bool bit_is_one)
{
    // Active low on the Wiegand lines, so drive GPIO high on the selected leg.
    if (bit_is_one)
    {
        gpio_put(tx_pin_d0_, 0);
        gpio_put(tx_pin_d1_, 1);
    }
    else
    {
        gpio_put(tx_pin_d0_, 1);
        gpio_put(tx_pin_d1_, 0);
    }
}

bool WiegandPort::transmit(const uint8_t *data, uint32_t bit_count, uint32_t bit_time_us,
                           uint32_t interbit_time_us)
{
    if (!data || bit_count == 0 || bit_count > kTxBufferBytes * 8)
    {
        return false;
    }
    if (tx_active_)
    {
        return false;  // busy
    }

    // Copy payload (little-endian bit order: LSB of first byte first).
    memset(tx_buffer_, 0, sizeof(tx_buffer_));
    const uint32_t byte_count = (bit_count + 7) / 8;
    memcpy(tx_buffer_, data, byte_count);

    tx_bits_ = bit_count;
    tx_bit_index_ = 0;
    tx_bit_time_us_ = bit_time_us;
    tx_interbit_time_us_ = interbit_time_us;
    tx_state_ = TxState::Pulse;
    tx_active_ = true;

    const uint8_t first_byte = tx_buffer_[0];
    const bool first_bit_is_one = first_byte & 0x1;
    drive_bit(first_bit_is_one);

    if (!add_repeating_timer_us(tx_bit_time_us_, tx_timer_trampoline, this, &tx_timer_))
    {
        tx_active_ = false;
        tx_state_ = TxState::Idle;
        drive_idle();
        return false;
    }
    return true;
}
