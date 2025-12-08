#pragma once

#include <cstddef>
#include <cstdint>

// Raw capture of a single Wiegand RX frame along with timing metadata.
struct RxMessage
{
    uint8_t port_id;      // 0 = A, 1 = B, 2 = C
    uint32_t bit_count;   // number of bits captured

    // Pulse width statistics (active-low pulses on either line), in timer ticks.
    uint32_t pulse_min;
    uint32_t pulse_avg;
    uint32_t pulse_max;

    // Gap between pulses, in timer ticks.
    uint32_t inter_min;
    uint32_t inter_avg;
    uint32_t inter_max;

    uint8_t data_bytes;    // length of data[] in bytes
    uint8_t data[32];      // up to 256 bits, MSB-first, right-aligned
};

// Fixed-size ring buffer for recent RX messages shared across ports.
class RxLogBuffer
{
public:
    RxLogBuffer();

    void clear();
    bool push(const RxMessage &msg);                       // overwrites oldest when full
    size_t size() const;
    size_t capacity() const;

    // Read back messages starting from newest. offset 0 returns most recent.
    bool get_from_newest(size_t offset, RxMessage &out) const;

    // Copy messages in FIFO order (oldest first) into caller-provided array.
    // Returns number of entries written (up to max).
    size_t copy_fifo(RxMessage *out, size_t max) const;

private:
    static constexpr size_t kCapacity = 5;
    RxMessage ring_[kCapacity];
    size_t head_;
    size_t count_;
};

extern RxLogBuffer g_rx_log_buffer;
