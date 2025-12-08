#pragma once

#include <cstddef>
#include <cstdint>

// Returns true if the bit at bit_index (0 = MSB of first byte) is 1.
inline bool bitutils_read_bit_msb(const uint8_t *data, uint32_t bit_index)
{
    const uint32_t byte_idx = bit_index / 8;
    const uint32_t bit_in_byte = 7 - (bit_index % 8);
    return ((data[byte_idx] >> bit_in_byte) & 0x1u) != 0;
}

// Sets the bit at bit_index (0 = MSB of first byte) to 1.
inline void bitutils_set_bit_msb(uint8_t *data, uint32_t bit_index)
{
    const uint32_t byte_idx = bit_index / 8;
    const uint32_t bit_in_byte = 7 - (bit_index % 8);
    data[byte_idx] |= static_cast<uint8_t>(1u << bit_in_byte);
}

// Formats a right-aligned, MSB-first bit buffer as hex with a "0x" prefix.
// Returns true on success.
bool bitutils_format_hex_msb(const uint8_t *data, uint32_t bit_count, char *out, size_t out_len);

