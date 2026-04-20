#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/linearalloc.h>
#include <crankshaft/alloc.h>
#include <crankshaft/util.h>

struct CS_LinearAllocator {
    int size;
    int current;
    struct CS_LinearAllocator *next;
};

static struct CS_LinearAllocator *privateAlloc(int size) {
    if( size < 0 ) {
        CS_LOG_ERROR("Size must be bigger than 0 on a linear allocator.");
        return NULL;
    }
    struct CS_LinearAllocator *returnValue = CS_alloc(sizeof(struct CS_LinearAllocator) + size);
    if( returnValue == NULL ) {
        CS_LOG_ERROR("OOM Allocating a linear allocator.");
        return NULL;
    }

    returnValue->size = size;
    returnValue->current = 0;
    returnValue->next = NULL;
    return returnValue;
}

inline static void *privateTake(struct CS_LinearAllocator *linearAllocator, int size, int alignment ) {
    char *root = (char*)(linearAllocator + 1);
    char *base = root + linearAllocator->current;
    char *aligned = (char*)CS_alignVoid(base, alignment);
    if( aligned + size > root + linearAllocator->size ) return NULL;
    linearAllocator->current = aligned - root + size;
    return aligned;
}

void *CS_linearTake(struct CS_LinearAllocator *linearAllocator, int size, int alignment ) {
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

void *CS_linearTakeZero(struct CS_LinearAllocator *linearAllocator, int size, int alignment ) {
    void *returnValue = CS_linearTake( linearAllocator, size, alignment );
    if( returnValue ) memset( returnValue, 0, size );
    return returnValue;
}

char *CS_linearCopyString(struct CS_LinearAllocator *linearAllocator, const char *string ) {
    int len = strlen( string );
    void *returnValue = CS_linearTake( linearAllocator, len+1, sizeof(void*) );
    if( returnValue ) strlcpy( returnValue, string, len + 1 );
    return returnValue;
}

void CS_linearReset( struct CS_LinearAllocator *linearAllocator ) {
    while(linearAllocator) {
        linearAllocator->current = 0;
        linearAllocator = linearAllocator->next;
    }
}

struct CS_LinearAllocator *CS_linearInit( int size ) {
    return privateAlloc(size);
}

void CS_linearFree( struct CS_LinearAllocator *linearAllocator ) {
    while(linearAllocator) {
        void *freeMe = (void*)linearAllocator;
        linearAllocator = linearAllocator->next;
        CS_free(freeMe);
    }
}
