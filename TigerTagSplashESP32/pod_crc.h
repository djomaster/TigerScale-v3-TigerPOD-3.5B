#pragma once
#include <stdint.h>
#include <stddef.h>
// ArduinoJson writer: same CRC32 as PodPolicy, without a full JSON String copy.
struct PodCrcWriter {
    uint32_t crc=~0u;size_t count=0;
    size_t write(uint8_t c) {
        crc^=c;for(int b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);
        ++count;return 1;
    }
    size_t write(const uint8_t* p,size_t n){for(size_t i=0;i<n;i++)write(p[i]);return n;}
    uint32_t value()const{return ~crc;}
};
