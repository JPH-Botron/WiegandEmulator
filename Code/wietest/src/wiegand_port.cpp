#include "wiegand_port.h"

#include <cstring>
#include <climits>
#include <cstdio>
#include <hardware/gpio.h>
#include "bit_utils.h"
#include "terminal.h"
#include "wiegand_rx_log.h"

namespace {

// Copy a right-aligned bitstream from src into dst, keeping MSB-first ordering.
// bit_count bits (up to dst_len * 8) are taken from the least-significant bits
// of src (using data_bytes to determine width).
bool copy_right_aligned_bits(const uint8_t *src, size_t data_bytes, uint32_t bit_count,
                             uint8_t *dst, size_t dst_len)
{
    if (!src || !dst || bit_count == 0)
    {
        return false;
    }
    const uint32_t needed_bytes = (bit_count + 7) / 8;
    if (needed_bytes > dst_len)
    {
        return false;
    }
    const uint32_t dst_bits = needed_bytes * 8;
    const uint32_t available_bits = static_cast<uint32_t>(data_bytes * 8);
    if (bit_count > available_bits)
    {
        return false;
    }

    std::memset(dst, 0, needed_bytes);
    const uint32_t dst_offset = dst_bits - bit_count;
    const uint32_t src_offset = available_bits - bit_count;
    for (uint32_t i = 0; i < bit_count; ++i)
    {
        const uint32_t src_bit_index = src_offset + i;
        const uint32_t dst_bit_index = dst_offset + i;
        if (bitutils_read_bit_msb(src, src_bit_index))
        {
            bitutils_set_bit_msb(dst, dst_bit_index);
        }
    }
    return true;
}

} // namespace

WiegandPort::WiegandPort(PIO pio, uint sm, uint irq_index, uint pin_base_d0, uint port_id,
                         uint tx_pin_d0, uint tx_pin_d1, uint led_pin)
    : pio_(pio),
      sm_(sm),
      irq_index_(irq_index),
      pin_base_d0_(pin_base_d0),
      port_id_(port_id),
      tx_pin_d0_(tx_pin_d0),
      tx_pin_d1_(tx_pin_d1),
      led_pin_(led_pin),
      buffer_{},
      count_(0),
      last_transition_ms_(0),
      tx_timer_{},
      tx_active_(false),
      tx_state_(TxState::Idle),
      tx_bits_(0),
      tx_bytes_(0),
      tx_bit_index_(0),
      tx_bit_time_us_(0),
      tx_interbit_time_us_(0),
      tx_buffer_{},
      led_off_deadline_ms_(0) {}

void WiegandPort::init(uint program_offset, float clk_div)
{
    wiegand_rx2_program_init(pio_, sm_, program_offset, pin_base_d0_, clk_div);
    // Enable IRQ when this SM's RX FIFO has data.
    pio_set_irq0_source_enabled(
        pio_, static_cast<pio_interrupt_source_t>(pis_sm0_rx_fifo_not_empty + sm_), true);
    pio_sm_set_enabled(pio_, sm_, true);

    pinMode(tx_pin_d0_, OUTPUT);
    pinMode(tx_pin_d1_, OUTPUT);
    drive_idle();
    pinMode(led_pin_, OUTPUT);
    gpio_put(led_pin_, 1); // idle off (low-true)
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

    // Snapshot count so we only process the records that were present on entry.
    uint32_t local_count = count_;
    if (local_count > kBufferCapacity)
    {
        local_count = kBufferCapacity;
    }

    uint32_t prev_levels = 0x3; // assume idle high on both lines
    uint32_t last_fall_ts[2] = {0, 0};
    bool in_low[2] = {false, false};
    uint32_t min_low[2] = {UINT32_MAX, UINT32_MAX};
    uint32_t max_low[2] = {0, 0};
    uint64_t sum_low[2] = {0, 0};
    uint32_t count_low[2] = {0, 0};
    uint32_t min_low_any = UINT32_MAX;
    uint32_t max_low_any = 0;
    uint64_t sum_low_any = 0;
    uint32_t count_low_any = 0;
    uint32_t min_inter = UINT32_MAX;
    uint32_t max_inter = 0;
    uint64_t sum_inter = 0;
    uint32_t count_inter = 0;
    uint32_t last_rise_ts_any = 0;
    bool have_last_rise = false;
    const uint32_t wrap = (1u << 30);
    uint8_t bit_stream[kMaxBits] = {0}; // raw bits as seen, MSB-first
    uint32_t bit_count = 0;

    for (uint32_t i = 0; i < local_count; ++i)
    {
        const uint32_t word = buffer_[i];
        const uint32_t ts = word >> 2;
        const uint32_t levels = word & 0x3;

        // Track low pulse durations per line.
        for (int line = 0; line < 2; ++line)
        {
            const uint32_t mask = 1u << line;
            const bool was_high = (prev_levels & mask) != 0;
            const bool now_high = (levels & mask) != 0;
            if (was_high && !now_high)
            {
                // Falling edge: start of low pulse.
                if (have_last_rise)
                {
                    const uint32_t inter = (last_rise_ts_any >= ts)
                                               ? (last_rise_ts_any - ts)
                                               : (last_rise_ts_any + wrap - ts);
                    if (inter < min_inter)
                    {
                        min_inter = inter;
                    }
                    if (inter > max_inter)
                    {
                        max_inter = inter;
                    }
                    sum_inter += inter;
                    count_inter += 1;
                }
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
                if (pulse < min_low_any)
                {
                    min_low_any = pulse;
                }
                if (pulse > max_low_any)
                {
                    max_low_any = pulse;
                }
                sum_low_any += pulse;
                count_low_any += 1;
                in_low[line] = false;

                // Record bit: D0 pulse -> 0, D1 pulse -> 1
                if (bit_count < kMaxBits)
                {
                    bit_stream[bit_count] = (line == 1) ? 1 : 0;
                }
                bit_count++;
                last_rise_ts_any = ts;
                have_last_rise = true;
            }
        }
        prev_levels = levels;
    }

    const uint32_t avg_any = (count_low_any > 0)
                                 ? static_cast<uint32_t>(sum_low_any / count_low_any)
                                 : 0;
    const uint32_t avg_inter = (count_inter > 0)
                                   ? static_cast<uint32_t>(sum_inter / count_inter)
                                   : 0;

    const uint32_t captured_bits = (bit_count > kMaxBits) ? kMaxBits : bit_count;
    const bool truncated = (bit_count > kMaxBits);
    const uint32_t byte_len = (captured_bits + 7) / 8;
    uint8_t packed[kTxBufferBytes] = {0};
    if (captured_bits > 0 && byte_len <= kTxBufferBytes)
    {
        const uint32_t total_bits = byte_len * 8;
        const uint32_t offset = total_bits - captured_bits;
        for (uint32_t i = 0; i < captured_bits; ++i)
        {
            if (bit_stream[i])
            {
                bitutils_set_bit_msb(packed, offset + i);
            }
        }
    }

    const char port_letter = static_cast<char>('A' + port_id_);
    char summary[96];
    std::snprintf(summary, sizeof(summary), "rx %c %lub%s %lu/%lu/%lu %lu/%lu/%lu", port_letter,
                  static_cast<unsigned long>(captured_bits), truncated ? "+" : "",
                  static_cast<unsigned long>((count_low_any > 0) ? min_low_any : 0),
                  static_cast<unsigned long>(avg_any),
                  static_cast<unsigned long>((count_low_any > 0) ? max_low_any : 0),
                  static_cast<unsigned long>((count_inter > 0) ? min_inter : 0),
                  static_cast<unsigned long>(avg_inter),
                  static_cast<unsigned long>((count_inter > 0) ? max_inter : 0));

    // Emit captured bits in hex.
    char hexline[2 * kTxBufferBytes + 3]; // "0x" + 2 chars per byte + null
    if (!bitutils_format_hex_msb(packed, captured_bits, hexline, sizeof(hexline)))
    {
        std::snprintf(hexline, sizeof(hexline), "0x");
    }
    // Choose display color per port.
    uint16_t color = TFT_GREEN;
    if (port_id_ == 1)
    {
        color = TFT_MAGENTA; // high contrast on black
    }
    else if (port_id_ == 2)
    {
        color = TFT_CYAN;
    }
    terminalSetColor(color);
    terminalAddLine(summary);
    terminalAddIndentedLine(hexline);
    terminalResetColor();

    // Stash the raw message and timing into the shared RX log buffer.
    RxMessage msg{};
    msg.port_id = port_id_;
    msg.bit_count = captured_bits;
    msg.pulse_min = (count_low_any > 0) ? min_low_any : 0;
    msg.pulse_avg = avg_any;
    msg.pulse_max = (count_low_any > 0) ? max_low_any : 0;
    msg.inter_min = (count_inter > 0) ? min_inter : 0;
    msg.inter_avg = avg_inter;
    msg.inter_max = (count_inter > 0) ? max_inter : 0;
    msg.data_bytes = static_cast<uint8_t>(byte_len);
    if (msg.data_bytes > sizeof(msg.data))
    {
        msg.data_bytes = sizeof(msg.data); // truncate defensively
    }
    std::memcpy(msg.data, packed, msg.data_bytes);
    g_rx_log_buffer.push(msg);

    trigger_led();
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
        const uint32_t bit_offset = (tx_bytes_ * 8) - tx_bits_;
        const uint32_t bit_index = bit_offset + tx_bit_index_;
        const bool bit_is_one = bitutils_read_bit_msb(tx_buffer_, bit_index);
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

bool WiegandPort::transmit(const uint8_t *data, size_t data_bytes, uint32_t bit_count,
                           uint32_t bit_time_us, uint32_t interbit_time_us)
{
    if (!data || bit_count == 0 || bit_count > kMaxBits)
    {
        return false;
    }
    if (data_bytes == 0)
    {
        return false;
    }
    if (tx_active_)
    {
        return false;  // busy
    }

    memset(tx_buffer_, 0, sizeof(tx_buffer_));
    if (!copy_right_aligned_bits(data, data_bytes, bit_count, tx_buffer_, sizeof(tx_buffer_)))
    {
        return false;
    }

    tx_bits_ = bit_count;
    tx_bytes_ = (bit_count + 7) / 8;
    tx_bit_index_ = 0;
    tx_bit_time_us_ = bit_time_us;
    tx_interbit_time_us_ = interbit_time_us;
    tx_state_ = TxState::Pulse;
    tx_active_ = true;

    const uint32_t first_bit_index = (tx_bytes_ * 8) - tx_bits_;
    const bool first_bit_is_one = bitutils_read_bit_msb(tx_buffer_, first_bit_index);
    drive_bit(first_bit_is_one);

    if (!add_repeating_timer_us(tx_bit_time_us_, tx_timer_trampoline, this, &tx_timer_))
    {
        tx_active_ = false;
        tx_state_ = TxState::Idle;
        drive_idle();
        return false;
    }
    // Log transmit summary and hex to serial and LCD terminal.
    const char port_letter = static_cast<char>('A' + port_id_);
    char summary[96];
    std::snprintf(summary, sizeof(summary), "tx %c %lub %lu %lu", port_letter,
                  static_cast<unsigned long>(bit_count),
                  static_cast<unsigned long>(tx_bit_time_us_),
                  static_cast<unsigned long>(tx_interbit_time_us_));

    char hexline[2 * kTxBufferBytes + 3]; // "0x" + 2 chars per byte + null
    if (!bitutils_format_hex_msb(tx_buffer_, bit_count, hexline, sizeof(hexline)))
    {
        std::snprintf(hexline, sizeof(hexline), "0x");
    }

    // Use per-port color for TX logs on the terminal.
    uint16_t color = TFT_GREEN;
    if (port_id_ == 1)
    {
        color = TFT_MAGENTA; // high contrast on black
    }
    else if (port_id_ == 2)
    {
        color = TFT_CYAN;
    }
    terminalSetColor(color);
    terminalAddLine(summary);
    terminalAddIndentedLine(hexline);
    terminalResetColor();
    Serial.println(summary);
    Serial.println(hexline);
    trigger_led();
    return true;
}

void WiegandPort::tick()
{
    if (led_off_deadline_ms_ != 0 && static_cast<int32_t>(led_off_deadline_ms_ - millis()) <= 0)
    {
        gpio_put(led_pin_, 1); // off (low-true)
        led_off_deadline_ms_ = 0;
    }
}

void WiegandPort::trigger_led(uint32_t duration_ms)
{
    led_off_deadline_ms_ = millis() + duration_ms;
    gpio_put(led_pin_, 0); // on (low-true)
}
