#include <stdlib.h>
#include <stdio.h>

#include <crankshaft/hash.h>
#include <stdint.h>

//#define PRIME 31
#define PRIME 37

uint32_t CS_hash(const char *str) {
    uint32_t accumulator = 27;
    const unsigned char *in = (const unsigned char *)str;
    while(*in!=0) {
        accumulator = PRIME * accumulator + *in++;
    } 
    return accumulator;
}

uint32_t CS_hashBin(const char *blob, int32_t length) {
    uint32_t accumulator = 27;
    const unsigned char *in = (const unsigned char *)blob;
    const unsigned char *end = in + length;
    while(in<end) {
        accumulator = PRIME * accumulator + *in++;
    } 
    return accumulator;
}

