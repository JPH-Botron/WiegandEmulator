#include "bit_utils.h"

namespace {

char hexchar(uint8_t nib)
{
    return (nib < 10) ? static_cast<char>('0' + nib) : static_cast<char>('a' + (nib - 10));
}

} // namespace

bool bitutils_format_hex_msb(const uint8_t *data, uint32_t bit_count, char *out, size_t out_len)
{
    if (!out || out_len < 3)
    {
        return false;
    }
    out[0] = '0';
    out[1] = 'x';
    size_t pos = 2;
    if (bit_count == 0)
    {
        out[pos] = '\0';
        return true;
    }
    if (!data)
    {
        return false;
    }

    const uint32_t byte_len = (bit_count + 7) / 8;
    const uint32_t nibble_count = (bit_count + 3) / 4;
    // We right-align the message to the least significant bits of the buffer.
    const uint32_t total_bits = byte_len * 8;
    const uint32_t start_bit = total_bits - (nibble_count * 4);

    if (out_len < pos + nibble_count + 1)
    {
        return false;
    }

    for (uint32_t n = 0; n < nibble_count; ++n)
    {
        uint8_t nib = 0;
        for (int k = 0; k < 4; ++k)
        {
            const uint32_t bit_index = start_bit + (n * 4) + static_cast<uint32_t>(k);
            nib = static_cast<uint8_t>((nib << 1) |
                                       (bitutils_read_bit_msb(data, bit_index) ? 1 : 0));
        }
        out[pos++] = hexchar(nib);
    }
    out[pos] = '\0';
    return true;
}
