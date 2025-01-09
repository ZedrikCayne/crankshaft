#include <stdlib.h>
#include <stdio.h>

#include "crankshaft/alloc.h"
#include "crankshaft/logger.h"

#include "crankshaft/hash.h"

//#define PRIME 31
#define PRIME 37

unsigned int CS_hash(const char *str) {
    unsigned int accumulator = 27;
    const unsigned char *in = (const unsigned char *)str;
    while(*in!=0) {
        accumulator = PRIME * accumulator + *in++;
    } 
    return accumulator;
}

unsigned int CS_hashBin(const char *blob, int length) {
    unsigned int accumulator = 27;
    const unsigned char *in = (const unsigned char *)blob;
    const unsigned char *end = in + length;
    while(in<end) {
        accumulator = PRIME * accumulator + *in++;
    } 
    return accumulator;
}

