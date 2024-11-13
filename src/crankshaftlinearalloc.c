#include <stdlib.h>
#include <stdio.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftlinearalloc.h"
#include "crankshaftalloc.h"
#include "crankshaftutil.h"

struct LinearAllocator {
    int size;
    int current;
    struct LinearAllocator *next;
};

static struct LinearAllocator *privateAlloc(int size) {
    if( size < 0 ) {
        CS_LOG_ERROR("Size must be bigger than 0 on a linear allocator.");
        return NULL;
    }
    struct LinearAllocator *returnValue = CS_alloc(sizeof(struct LinearAllocator) + size);
    if( returnValue == NULL ) {
        CS_LOG_ERROR("OOM Allocating a linear allocator.");
        return NULL;
    }

    returnValue->size = size;
    returnValue->current = 0;
    returnValue->next = NULL;
    return returnValue;
}

inline static void *privateTake(struct LinearAllocator *linearAllocator, int size, int alignment ) {
    char *root = (char*)(linearAllocator + 1);
    char *base = root + linearAllocator->current;
    char *aligned = (char*)CS_alignVoid(base, alignment);
    if( aligned + size > root + linearAllocator->size ) return NULL;
    linearAllocator->current = aligned - root + size;
    return aligned;
}

void *CS_takeLinear(void *voidAllocator, int size, int alignment ) {
    struct LinearAllocator *linearAllocator = (struct LinearAllocator *)voidAllocator;
    if( size > linearAllocator->size ) {
        CS_LOG_ERROR("Cannot take %d out of an allocator sized of %d", size, linearAllocator->size);
    }
    void *returnValue;
    while( (returnValue = privateTake(linearAllocator, size, alignment)) == NULL ) {
        if( linearAllocator->current == 0 ) {
            CS_LOG_ERROR("Even though the size requested is less than the allocator size, we cannot actually allocate this with the alignment provided.");
            return NULL;
        }
        if( linearAllocator->next == NULL ) {
            linearAllocator->next = privateAlloc( linearAllocator->size );
            if( linearAllocator->next == NULL ) return NULL;
        }
        linearAllocator = linearAllocator->next;
    }
    return returnValue;
}

void CS_resetLinear(void *voidAllocator) {
    struct LinearAllocator *linearAllocator = (struct LinearAllocator *)voidAllocator;
    while(linearAllocator) {
        linearAllocator->current = 0;
        linearAllocator = linearAllocator->next;
    }
}

void *CS_allocLinearAllocator( int size ) {
    return privateAlloc(size);
}

void CS_freeLinearAllocator( void *voidAllocator ) {
    struct LinearAllocator *linearAllocator = (struct LinearAllocator *)voidAllocator;
    while(linearAllocator) {
        void *freeMe = (void*)linearAllocator;
        linearAllocator = linearAllocator->next;
        CS_free(freeMe);
    }
}
