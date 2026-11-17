#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// Pure policy shared with the host fault-injection tests.
namespace PodPolicy {
inline uint16_t capacity(uint8_t size) { return size == 0x0f ? 144 : size == 0x11 ? 504 : size == 0x13 ? 888 : 0; }
inline bool blank(const uint8_t* p, size_t n) {
    const uint8_t headers[][8] = {{0}, {0x6c,0x41,0xa2,0xe1,0,0,0,0}, {3,0,0xfe,0}, {1,3,0xa0,0x0c,0x34,3,0,0xfe}, {3,4,0xd8,0,0,0,0xfe,0}};
    for (const auto& h : headers) {
        bool ok = n >= 8 && !memcmp(p,h,8);
        for (size_t i=8; ok && i<n; ++i) ok = p[i] == 0;
        if (ok) return true;
    }
    return false;
}
inline bool recoverable(const uint8_t* actual, const uint8_t* before, const uint8_t* target) {
    // A torn page is quarantined, not silently overwritten. Lost ACK is safe.
    for (size_t i=0;i<144;i+=4)
        if (memcmp(actual+i,before+i,4) && memcmp(actual+i,target+i,4)) return false;
    return true;
}
inline uint32_t checksum(const void* data, size_t n) {
    uint32_t crc=~0u; const uint8_t* p=(const uint8_t*)data;
    while(n--) { crc ^= *p++; for(int b=0;b<8;b++) crc=(crc>>1)^((0u-(crc&1))&0xedb88320u); }
    return ~crc;
}
}
