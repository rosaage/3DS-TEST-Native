#include "../source/cpp_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Build a report independently from the decoder. The all-zero CRC check is
// also checked against the standard CRC-8/SMBUS "123456789" check value.
static size_t report(uint8_t *packet, unsigned x, unsigned y,
                     uint8_t released, bool longHeader)
{
    packet[0] = 0xA5;
    packet[1] = 1;
    packet[2] = longHeader ? 0x40 : 6;
    size_t header = longHeader ? 4 : 3;
    if (longHeader) packet[3] = 6;
    uint8_t *data = packet + header;
    data[0] = 0x10;
    data[1] = x;
    data[2] = (x >> 8) | (y << 4);
    data[3] = y >> 4;
    data[4] = released | 19;
    data[5] = 0;
    uint8_t crc = 0;
    for (size_t i = 0; i < header + 6; ++i) crc = cppCrcByte(crc, packet[i]);
    packet[header + 6] = crc;
    return header + 7;
}

int main(void)
{
    uint8_t crc = 0;
    const char *check = "123456789";
    for (int i = 0; i < 9; ++i) crc = cppCrcByte(crc, check[i]);
    assert(crc == 0xF4);
    uint8_t packet[11], ring[19];
    CppSample sample;
    for (unsigned x = 0; x <= 4095; x += 273) {
        unsigned y = 4095 - x;
        for (unsigned buttons = 0; buttons < 8; ++buttons) {
            uint8_t released = ((buttons & CPP_R) ? 0 : 0x80)
                             | ((buttons & CPP_ZL) ? 0 : 0x20)
                             | ((buttons & CPP_ZR) ? 0 : 0x40);
            for (unsigned longHeader = 0; longHeader < 2; ++longHeader) {
                size_t size = report(packet, x, y, released, longHeader);
                for (unsigned offset = 0; offset < sizeof(ring); ++offset) {
                    memset(ring, 0, sizeof(ring));
                    for (size_t i = 0; i < size; ++i)
                        ring[(offset + i) % sizeof(ring)] = packet[i];
                    assert(cppDecodePacket(ring, sizeof(ring), offset, size, &sample));
                    assert(sample.x == x && sample.y == y);
                    assert(sample.buttons == buttons && sample.battery == 19);
                    ring[(offset + 5) % sizeof(ring)] ^= 1;
                    assert(!cppDecodePacket(ring, sizeof(ring), offset, size, &sample));
                }
            }
        }
    }
    size_t size = report(packet, 2048, 2048, 0xE0, false);
    assert(cppDecodePacket(packet, size, 0, size, &sample));
    assert(sample.x == 2048 && sample.y == 2048 && sample.buttons == 0);
    for (size_t n = 0; n < size; ++n)
        assert(!cppDecodePacket(packet, size, 0, n, &sample));
    assert(!cppDecodePacket(packet, size, size, size, &sample));
    assert(!cppDecodePacket(packet, 0, 0, size, &sample));
    assert(!cppDecodePacket(packet, size, 0, size + 1, &sample));
    assert(!cppDecodePacket(NULL, size, 0, size, &sample));
    assert(!cppDecodePacket(packet, size, 0, size, NULL));
    // A valid CRC must not make a non-input response acceptable.
    packet[3] = 0x11;
    crc = 0;
    for (size_t i = 0; i < size - 1; ++i) crc = cppCrcByte(crc, packet[i]);
    packet[size - 1] = crc;
    assert(!cppDecodePacket(packet, size, 0, size, &sample));
    puts("CPP packet tests passed: axes, buttons, wrapping, CRC and invalid data");
    return 0;
}
