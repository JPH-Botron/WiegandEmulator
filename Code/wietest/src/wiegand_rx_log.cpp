#include "wiegand_rx_log.h"

#include <cstring>

RxLogBuffer g_rx_log_buffer;

RxLogBuffer::RxLogBuffer() : ring_{}, head_(0), count_(0) {}

void RxLogBuffer::clear()
{
    head_ = 0;
    count_ = 0;
}

bool RxLogBuffer::push(const RxMessage &msg)
{
    ring_[head_] = msg;
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity)
    {
        count_ += 1;
    }
    return true;
}

size_t RxLogBuffer::size() const
{
    return count_;
}

size_t RxLogBuffer::capacity() const
{
    return kCapacity;
}

bool RxLogBuffer::get_from_newest(size_t offset, RxMessage &out) const
{
    if (offset >= count_)
    {
        return false;
    }
    // head_ points to next write slot; newest entry is head_-1 with wrap.
    size_t idx = (head_ + kCapacity - 1 - offset) % kCapacity;
    out = ring_[idx];
    return true;
}

size_t RxLogBuffer::copy_fifo(RxMessage *out, size_t max) const
{
    if (!out || max == 0 || count_ == 0)
    {
        return 0;
    }
    const size_t n = (count_ < max) ? count_ : max;
    const size_t start = (head_ + kCapacity - count_) % kCapacity; // oldest
    for (size_t i = 0; i < n; ++i)
    {
        const size_t idx = (start + i) % kCapacity;
        out[i] = ring_[idx];
    }
    return n;
}
