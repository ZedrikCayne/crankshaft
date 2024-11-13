#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "crankshaftalloc.h"
#include "crankshaftslaballoc.h"
#include "crankshaftstringbuilder.h"
#include "crankshaftlogger.h"

struct CrankshaftSlabAllocItem {
    struct CrankshaftSlabAllocItem *next;
};

#pragma GCC diagnostic ignored "-Wformat-truncation"
#define SLAB_NAME_MAX 32
struct CrankshaftSlabAlloc {
    int size;
    int capacity;
    struct CrankshaftSlabAllocItem *head;
    struct CrankshaftSlabAlloc *nextSlab;
    char *buffer;
    char *bufferEnd;
    pthread_mutex_t slabMutex;
    char name[SLAB_NAME_MAX];
};

static bool freeSlab( struct CrankshaftSlabAlloc *slab ) {
    struct CrankshaftSlabAlloc *slabToFree = slab;
    struct CrankshaftSlabAlloc *nextSlab;
    while( slabToFree != NULL ) {
        nextSlab = slabToFree->nextSlab;
        slabToFree->nextSlab = NULL;
        CS_free( slabToFree->buffer );
        CS_free( slabToFree );
        slabToFree->buffer = NULL;
        slabToFree = nextSlab;
    }
    return false;
}

static struct CrankshaftSlabAlloc *initSlabAlloc( int size, int count, int alignment ) {
    int realSize = (size % alignment == 0) ?
        size :
        size + ( alignment - (size % alignment) );
    struct CrankshaftSlabAlloc *returnValue = CS_alloc( sizeof(struct CrankshaftSlabAlloc) );
    if( returnValue == NULL ) {
        return NULL;
    }
    returnValue->buffer = CS_alloc( realSize * count );

    if( returnValue->buffer == NULL ) {
        CS_free(returnValue);
        return NULL;
    }
    returnValue->size = realSize;
    returnValue->capacity = count;
    returnValue->head = (struct CrankshaftSlabAllocItem *)returnValue->buffer;
    returnValue->nextSlab = NULL;
    returnValue->bufferEnd = returnValue->buffer + ( returnValue->size * returnValue->capacity );

    for( int i = 0; i < count; ++i ) { 
        struct CrankshaftSlabAllocItem *next = (struct CrankshaftSlabAllocItem *)(returnValue->buffer + ( ( i + 1 ) * realSize ));
        struct CrankshaftSlabAllocItem *current = (struct CrankshaftSlabAllocItem *)(returnValue->buffer + (i * realSize));
        if( (char*)next < returnValue->bufferEnd ) {
            current->next = next;
        } else {
            current = NULL;
        }
    }

    return returnValue;
}

void *CS_initSlabAlloc( const char *name, int size, int count, int alignment ) {
    if( alignment % CRANKSHAFT_MIN_ALIGNMENT ) {
        CS_LOG_ERROR("Alignment on a slab alloc must be a multiple of CRANKSHAFT_MIN_ALIGNMENT:%d", CRANKSHAFT_MIN_ALIGNMENT);
        return NULL;
    }
    if( name == NULL ) {
        CS_LOG_ERROR("Name your slab please.");
        return NULL;
    }
    int len = strlen(name);
    if( len > CRANKSHAFT_SLAB_NAME_MAX ) {
        CS_LOG_ERROR("Name of slab must be less than CRANKSHAFT_SLAB_NAME_MAX:%d", CRANKSHAFT_SLAB_NAME_MAX);
        return NULL;
    }
    struct CrankshaftSlabAlloc *returnValue = initSlabAlloc( size, count, alignment );
    if( returnValue != NULL ) {
        if( pthread_mutex_init( &returnValue->slabMutex, NULL ) < 0 ) {
            freeSlab( returnValue );
            returnValue = NULL;
        }
    }
    if( returnValue != NULL && name != NULL ) {
        strncpy( returnValue->name, name, len + 2 );
    }
    return returnValue;
}

bool CS_freeSlabAlloc( void *allocation ) {
    struct CrankshaftSlabAlloc *toFree = (struct CrankshaftSlabAlloc *)allocation;
    pthread_mutex_destroy( &toFree->slabMutex );
    return freeSlab( toFree );
}

void *CS_takeOne(void *voidSlab ) {
    struct CrankshaftSlabAlloc *slab = (struct CrankshaftSlabAlloc *)voidSlab;
    struct CrankshaftSlabAlloc *currentSlab = slab;
    pthread_mutex_lock(&slab->slabMutex);
    struct CrankshaftSlabAllocItem *returnValue = currentSlab->head;
    int slabDeep = 0;
    while( returnValue == NULL ) {
        ++slabDeep;
        if( currentSlab->nextSlab == NULL ) {
            //It is never wrong to use 4. (Minimum anyhow on systems that require int reads to be aligned)
            currentSlab->nextSlab = initSlabAlloc( currentSlab->size, currentSlab->capacity, CRANKSHAFT_MIN_ALIGNMENT );
            if( currentSlab->nextSlab == NULL ) {
                CS_LOG_ERROR("Slab %s failed to expand. OOM", currentSlab->name);
                goto RELEASE_LOCK;
            }
            snprintf( currentSlab->nextSlab->name, SLAB_NAME_MAX, "%s %d", slab->name, slabDeep );
        }
        currentSlab = currentSlab->nextSlab;
        returnValue = currentSlab->head;
    }
    currentSlab->head = returnValue->next;
RELEASE_LOCK:    
    pthread_mutex_unlock(&slab->slabMutex);
    return returnValue;
}

bool CS_returnOne(void *voidSlab, void *toReturn) {
    if( toReturn == NULL ) {
        CS_LOG_ERROR("Trying to free up a NULL");
        return true;
    }
    struct CrankshaftSlabAlloc *slab = (struct CrankshaftSlabAlloc *)voidSlab;
    struct CrankshaftSlabAlloc *currentSlab = slab;
    struct CrankshaftSlabAllocItem *itemToReturn = (struct CrankshaftSlabAllocItem *)toReturn;

    while( currentSlab != NULL && ((char*)toReturn < currentSlab->buffer || (char*)toReturn >= currentSlab->bufferEnd) ) {
        currentSlab = currentSlab->nextSlab;
    }
    if( currentSlab == NULL ) {
        CS_LOG_ERROR("Not a thing we can return to this slab.");
        return true;
    }
    if( ((char*)toReturn - currentSlab->buffer) % currentSlab->size != 0 ) {
        CS_LOG_ERROR("Pointer misaligned");
        return true;
    }

    pthread_mutex_lock(&slab->slabMutex);
    itemToReturn->next = currentSlab->head;
    currentSlab->head = itemToReturn;
    pthread_mutex_unlock(&slab->slabMutex);

    return false;
}

const char *CS_descSlabAlloc( void *allocation ) {
    struct CrankshaftSlabAlloc *slab = (struct CrankshaftSlabAlloc *)allocation;
    struct CS_StringBuilder *sb = CS_SB_create( 2048 );
    if( sb == NULL ) return NULL;
    pthread_mutex_lock(&slab->slabMutex);
    int totalSize = 0;
    int slabCount = 0;
    int totalFree = 0;
    int totalCapacity = 0;
    struct CrankshaftSlabAlloc *current = slab;
    struct CrankshaftSlabAllocItem *currentItem = NULL;
    while( current ) {
        totalSize += (current->size * current->capacity) + sizeof(struct CrankshaftSlabAlloc);
        ++slabCount;
        totalCapacity += current->capacity;
        currentItem = current->head;
        while( currentItem ) {
            ++totalFree;
            currentItem = currentItem->next;
        }
        current = current->nextSlab;
    }
    pthread_mutex_unlock(&slab->slabMutex);
    CS_SB_printf( sb, "Slab %s: %d bytes in %d slabs. %d slots available with %d free.",
            slab->name, totalSize, slabCount, totalCapacity, totalFree );
    return CS_SB_freeButReturnBuffer(sb);
}
