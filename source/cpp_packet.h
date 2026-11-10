// Circle Pad Pro wire format: https://3dbrew.org/wiki/Circle_Pad_Pro
// IR envelope: https://3dbrew.org/wiki/IRUSER_Shared_Memory
// Independent implementation, distributed under the repository's MIT license.
#ifndef CPP_PACKET_H
#define CPP_PACKET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t x, y;
    uint8_t buttons, battery;
} CppSample;

enum { CPP_R = 1, CPP_ZL = 2, CPP_ZR = 4 };

static inline uint8_t cppCrcByte(uint8_t crc, uint8_t value)
{
    crc ^= value;
    for (int bit = 0; bit < 8; ++bit)
        crc = (uint8_t)((crc << 1) ^ (crc & 0x80 ? 0x07 : 0));
    return crc;
}

static inline bool cppDecodePacket(const volatile uint8_t *ring, size_t capacity,
                                   size_t offset, size_t size, CppSample *out)
{
    if (!ring || !out || offset >= capacity || size < 4 || size > capacity)
        return false;
    // Input reports contain six bytes; accept either envelope length encoding.
    uint8_t packet[11];
    if (size > sizeof(packet)) return false;
    uint8_t crc = 0;
    for (size_t i = 0; i < size; ++i) {
        packet[i] = ring[(offset + i) % capacity];
        crc = cppCrcByte(crc, packet[i]);
    }
    if (crc || packet[0] != 0xA5) return false;
    size_t header = packet[2] & 0x40 ? 4 : 3;
    size_t payload = packet[2] & 0x3F;
    if (header == 4) payload = (payload << 8) | packet[3];
    if (payload != 6 || size != header + payload + 1) return false;
    const uint8_t *data = packet + header;
    if (data[0] != 0x10) return false;
    CppSample sample = {
        .x = data[1] | ((data[2] & 0x0F) << 8),
        .y = (data[2] >> 4) | (data[3] << 4),
        .battery = data[4] & 0x1F,
        .buttons = (!(data[4] & 0x80) ? CPP_R : 0)
                 | (!(data[4] & 0x20) ? CPP_ZL : 0)
                 | (!(data[4] & 0x40) ? CPP_ZR : 0)
    };
    *out = sample;
    return true;
}
#endif
