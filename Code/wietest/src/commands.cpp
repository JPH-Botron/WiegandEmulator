#include "commands.h"

#include <cstdlib>
#include <cstring>

#include "bit_utils.h"
#include "firmware_version.h"
#include "terminal.h"
#include "wiegand_rx_log.h"

namespace {

WiegandPort *g_ports = nullptr;
size_t g_port_count = 0;

int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool parse_hex_string(const char *hex, uint8_t *out, size_t out_cap, size_t &out_len)
{
    if (!hex) return false;
    const size_t len = std::strlen(hex);
    if (len == 0) return false;
    size_t padded_len = len;
    bool prepend_zero = false;
    if (padded_len % 2 != 0) { padded_len += 1; prepend_zero = true; }
    const size_t byte_len = padded_len / 2;
    if (byte_len > out_cap) return false;
    out_len = byte_len;
    size_t src_index = 0;
    for (size_t i = 0; i < byte_len; ++i)
    {
        int hi = 0;
        int lo = 0;
        if (prepend_zero && src_index == 0) hi = 0;
        else hi = hex_nibble(hex[src_index++]);
        lo = hex_nibble(hex[src_index++]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool cmd_ping(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Serial.println("OK");
    return true;
}

bool cmd_ver(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Serial.print("VER ");
    Serial.println(FIRMWARE_VERSION);
    return true;
}

bool cmd_help(int argc, char *argv[])
{
    (void)argc; (void)argv;
    Serial.println("Commands:");
    Serial.println("  ping");
    Serial.println("  ver");
    Serial.println("  help");
    Serial.println("  pins <a|b|c>");
    Serial.println("  getrx");
    Serial.println("  tx <a|b|c> <hexdata> [bits] [bit_us] [inter_us]");
    return true;
}

bool cmd_pins(int argc, char *argv[])
{
    if (argc < 2) { Serial.println("ERR usage: pins <a|b|c>"); return false; }
    const char port_char = argv[1][0];
    int port_index = -1;
    if (port_char == 'a') port_index = 0;
    else if (port_char == 'b') port_index = 1;
    else if (port_char == 'c') port_index = 2;
    else { Serial.println("ERR bad port"); return false; }
    if (!g_ports || static_cast<size_t>(port_index) >= g_port_count) { Serial.println("ERR bad port"); return false; }
    const uint d0_pin = g_ports[port_index].rx_pin_d0();
    const uint d1_pin = g_ports[port_index].rx_pin_d1();
    const int d0 = digitalRead(d0_pin);
    const int d1 = digitalRead(d1_pin);
    Serial.print("{\"port\":\""); Serial.print(port_char); Serial.print("\",\"d0\":"); Serial.print(d0);
    Serial.print(",\"d1\":"); Serial.print(d1); Serial.println("}");
    return true;
}

bool cmd_getrx(int argc, char *argv[])
{
    (void)argc; (void)argv;
    constexpr size_t kMaxMessages = 5;
    RxMessage msgs[kMaxMessages];
    const size_t n = g_rx_log_buffer.copy_fifo(msgs, kMaxMessages);

    Serial.print("[");
    for (size_t i = 0; i < n; ++i)
    {
        const RxMessage &m = msgs[i];
        if (i > 0) Serial.print(",");
        Serial.print("{\"port\":\""); Serial.print(static_cast<char>('a' + m.port_id));
        Serial.print("\",\"bits\":"); Serial.print(m.bit_count);
        Serial.print(",\"pulse\":["); Serial.print(m.pulse_min); Serial.print(","); Serial.print(m.pulse_avg); Serial.print(","); Serial.print(m.pulse_max);
        Serial.print("],\"gap\":["); Serial.print(m.inter_min); Serial.print(","); Serial.print(m.inter_avg); Serial.print(","); Serial.print(m.inter_max);
        Serial.print("],\"data\":\"");
        char hexline[2 * sizeof(m.data) + 3] = {};
        if (bitutils_format_hex_msb(m.data, m.bit_count, hexline, sizeof(hexline)))
        {
            Serial.print(hexline);
        }
        Serial.print("\"}");
    }
    Serial.println("]");

    if (n > 0) g_rx_log_buffer.clear();
    return true;
}

bool cmd_tx(int argc, char *argv[])
{
    if (argc < 3) { Serial.println("ERR usage: tx <a|b|c> <hexdata> [bits] [bit_us] [inter_us]"); return false; }

    const char port_char = argv[1][0];
    int port_index = (port_char == 'a') ? 0 : (port_char == 'b') ? 1 : (port_char == 'c') ? 2 : -1;
    if (port_index < 0 || !g_ports || static_cast<size_t>(port_index) >= g_port_count) { Serial.println("ERR bad port"); return false; }

    constexpr size_t kMaxTxBytes = 32;
    uint8_t tx_buf[kMaxTxBytes];
    std::memset(tx_buf, 0, sizeof(tx_buf)); // pad remaining bits with zeros
    size_t tx_len = 0;
    if (!parse_hex_string(argv[2], tx_buf, kMaxTxBytes, tx_len)) { Serial.println("ERR bad hex"); return false; }
    if (tx_len == 0) { Serial.println("ERR bad hex"); return false; }

    uint32_t bit_count = 26;
    if (argc >= 4)
    {
        bit_count = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
        if (bit_count == 0) { Serial.println("ERR bad bits"); return false; }
    }
    if (bit_count > 256)
    {
        Serial.println("ERR too many bits (max 256)");
        return false;
    }

    const uint32_t available_bits = static_cast<uint32_t>(tx_len * 8);
    if (bit_count > available_bits)
    {
        Serial.println("ERR not enough hex digits for requested bits");
        return false;
    }

    uint32_t bit_time_us = 100;
    if (argc >= 5) bit_time_us = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10));
    if (bit_time_us == 0) bit_time_us = 1;

    uint32_t interbit_us = 50;
    if (argc >= 6)
    {
        interbit_us = static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 10));
        if (interbit_us == 0) interbit_us = 1;
    }

    if (!g_ports[port_index].transmit(tx_buf, tx_len, bit_count, bit_time_us, interbit_us))
    {
        Serial.println("ERR transmit failed");
        return false;
    }

    return true;
}

SerialCommand kCommands[] = {
    {"ping",  cmd_ping},
    {"ver",   cmd_ver},
    {"help",  cmd_help},
    {"pins",  cmd_pins},
    {"getrx", cmd_getrx},
    {"tx",    cmd_tx},
};

} // namespace

void register_commands(SerialCommandProcessor &processor, WiegandPort *ports, size_t port_count)
{
    g_ports = ports;
    g_port_count = port_count;
    processor.set_commands(kCommands, sizeof(kCommands) / sizeof(kCommands[0]));
}
