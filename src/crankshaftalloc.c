#include <stdlib.h>
#include "crankshaftalloc.h"
#include "crankshaftrandom.h"

static int mallocFailRate = 0;
static int mallocFailSize = 0;

static struct CS_LCG_rand_state memoryRNG = {0xBADF00D};

#define MIN_FAIL_RATE 0
#define MAX_FAIL_RATE 100
void CS_setFailAlloc(int percentageOfTheTime) {
    mallocFailRate = percentageOfTheTime;
    if( mallocFailRate < MIN_FAIL_RATE ) mallocFailRate = MIN_FAIL_RATE;
    if( mallocFailRate > MAX_FAIL_RATE ) mallocFailRate = MAX_FAIL_RATE;
}

void CS_setMaxAlloc(int maxSize) {
    mallocFailSize = maxSize;
    if( maxSize < 0 ) maxSize = 0;
}

void *CS_alloc_detailled(unsigned long size,const char *file,int line) {
    if( mallocFailSize > 0 ) if( size > mallocFailSize ) return NULL;
    if( mallocFailRate > 0 ) if( (CS_LCG_rand(&memoryRNG)%MAX_FAIL_RATE) < mallocFailRate ) return NULL;
    return malloc(size);
}
void CS_free_detailled(void *freeMe,const char *file, int line) {
    return free(freeMe);
}
void *CS_realloc_detailled(void *reallocMe,unsigned long size,const char *file, int line) {
    if( mallocFailSize > 0 ) if( size > mallocFailSize ) return NULL;
    if( mallocFailRate > 0 ) if( (CS_LCG_rand(&memoryRNG)%MAX_FAIL_RATE) < mallocFailRate ) return NULL;
    return realloc(reallocMe,size);
}

