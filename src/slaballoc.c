#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <crankshaft/alloc.h>
#include <crankshaft/slaballoc.h>
#include <crankshaft/stringbuilder.h>
#include <crankshaft/logger.h>
#include <crankshaft/mutex.h>

struct SlabAllocItem {
    struct SlabAllocItem *next;
};

#define MIN_ITEM_SIZE sizeof( struct SlabAllocItem )

//#pragma GCC diagnostic ignored "-Wformat-truncation"
#define SLAB_NAME_MAX 32
struct CS_SlabAllocator {
    int size;
    int originalSize;
    int capacity;
    struct SlabAllocItem *head;
    struct CS_SlabAllocator *nextSlab;
    char *buffer;
    char *bufferEnd;
    pthread_mutex_t slabMutex;
    char name[SLAB_NAME_MAX];
    bool useMalloc;
};

static bool freeSlab( struct CS_SlabAllocator *slab ) {
    struct CS_SlabAllocator *slabToFree = slab;
    struct CS_SlabAllocator *nextSlab;
    while( slabToFree != NULL ) {
        nextSlab = slabToFree->nextSlab;
        slabToFree->nextSlab = NULL;
        slabToFree->useMalloc?free( slabToFree->buffer ):CS_free( slabToFree->buffer );
        slabToFree->buffer = NULL;
        slabToFree->useMalloc?free( slabToFree ):CS_free( slabToFree );
        slabToFree = nextSlab;
    }
    return false;
}

#define ALLOC_ITEM(_SLAB,_ITEM) ((struct SlabAllocItem*)(((_SLAB)->buffer)+((_ITEM)*((_SLAB)->size))))

static void resetItems( struct CS_SlabAllocator *slab ) {
    int count = slab->capacity;
    struct CS_SlabAllocator *slabToReset = slab;
    struct CS_SlabAllocator *nextSlab;
    while( slabToReset != NULL ) {
        nextSlab = slabToReset->nextSlab;
        for( int i = 0; i < count; ++i ) { 
            struct SlabAllocItem *current = ALLOC_ITEM(slabToReset,i);
            struct SlabAllocItem *next = (i+1>=count)?NULL:ALLOC_ITEM(slabToReset,i+1);
            current->next = next;
        }
        slab->head = ALLOC_ITEM(slab,0);
        slabToReset = nextSlab;
    }
}

static struct CS_SlabAllocator *initSlabAlloc( int size, int count, int alignment, bool useMalloc ) {
    if( size < sizeof(struct SlabAllocItem) ) size = sizeof(struct SlabAllocItem);
    int realSize = (size % alignment == 0) ?
        size :
        size + ( alignment - (size % alignment) );
    struct CS_SlabAllocator *returnValue = useMalloc?malloc( sizeof(struct CS_SlabAllocator) ):CS_alloc( sizeof(struct CS_SlabAllocator) );
    if( returnValue == NULL ) {
        return NULL;
    }
    returnValue->buffer = useMalloc?malloc( realSize * count ):CS_alloc( realSize * count );

    if( returnValue->buffer == NULL ) {
        useMalloc?free(returnValue):CS_free(returnValue);
        return NULL;
    }
    returnValue->size = realSize;
    returnValue->originalSize = size;
    returnValue->capacity = count;
    returnValue->head = (struct SlabAllocItem *)returnValue->buffer;
    returnValue->nextSlab = NULL;
    returnValue->bufferEnd = returnValue->buffer + ( returnValue->size * returnValue->capacity );
    returnValue->useMalloc = useMalloc;

    resetItems( returnValue );

    return returnValue;
}

static struct CS_SlabAllocator *privateSlabInit( const char *name, int size, int count, int alignment, bool useMalloc );
struct CS_SlabAllocator *CS_slabInitMalloc( const char *name, int size, int count, int alignment ) {
    return privateSlabInit(name,size,count,alignment,true);
}
struct CS_SlabAllocator *CS_slabInit( const char *name, int size, int count, int alignment ) {
    return privateSlabInit(name,size,count,alignment,false);
}
static struct CS_SlabAllocator *privateSlabInit( const char *name, int size, int count, int alignment, bool useMalloc ) {
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
    struct CS_SlabAllocator *returnValue = initSlabAlloc( size, count, alignment, useMalloc );
    if( returnValue != NULL ) {
        if( pthread_mutex_init( &returnValue->slabMutex, NULL ) ) {
            freeSlab( returnValue );
            returnValue = NULL;
        }
    }
    if( returnValue != NULL && name != NULL ) {
        strlcpy( returnValue->name, name, len + 2 );
    }
    return returnValue;
}

bool CS_slabFree( struct CS_SlabAllocator *allocation ) {
    if( allocation == NULL ) return true;
    pthread_mutex_destroy( &allocation->slabMutex );
    return freeSlab( allocation );
}

bool CS_slabReset( struct CS_SlabAllocator *allocation ) {
    if( allocation == NULL ) return true;

    pthread_mutex_lock( &allocation->slabMutex );
    resetItems( allocation );
    pthread_mutex_unlock( &allocation->slabMutex );
    return false;
}

void *CS_slabTake(struct CS_SlabAllocator *voidSlab ) {
    struct CS_SlabAllocator *slab = (struct CS_SlabAllocator *)voidSlab;
    struct CS_SlabAllocator *currentSlab = slab;
    pthread_mutex_lock(&slab->slabMutex);
    struct SlabAllocItem *returnValue = currentSlab->head;
    int slabDeep = 0;
    while( returnValue == NULL ) {
        ++slabDeep;
        if( currentSlab->nextSlab == NULL ) {
            //It is never wrong to use the min alignment. The size of each
            //item is already aligned to whatever the user wanted
            currentSlab->nextSlab = initSlabAlloc( currentSlab->size, currentSlab->capacity, CRANKSHAFT_MIN_ALIGNMENT, currentSlab->useMalloc );
            if( currentSlab->nextSlab == NULL ) {
                CS_LOG_ERROR("Slab %s failed to expand. OOM", currentSlab->name);
                goto RELEASE_LOCK;
            }
            snprintf( currentSlab->nextSlab->name, SLAB_NAME_MAX, "%.*s %d", SLAB_NAME_MAX - 12, slab->name, slabDeep );
        }
        currentSlab = currentSlab->nextSlab;
        returnValue = currentSlab->head;
    }
    currentSlab->head = returnValue->next;
RELEASE_LOCK:    
    pthread_mutex_unlock(&slab->slabMutex);
    return returnValue;
}

void *CS_slabTakeZero(struct CS_SlabAllocator *voidSlab ) {
    void *returnValue = CS_slabTake( voidSlab );
    if( returnValue ) memset( returnValue, 0, voidSlab->size );
    return returnValue;
}

void *CS_slabTakeCopy(struct CS_SlabAllocator *voidSlab, const void *source ) {
    void *returnValue = CS_slabTake( voidSlab );
    if( returnValue ) memcpy( returnValue, source, voidSlab->originalSize );
    return returnValue;
}

bool CS_slabReturn(struct CS_SlabAllocator *slab, void *toReturn) {
    if( toReturn == NULL ) {
        CS_LOG_ERROR("Trying to free up a NULL");
        return true;
    }
    struct CS_SlabAllocator *currentSlab = slab;
    struct SlabAllocItem *itemToReturn = (struct SlabAllocItem *)toReturn;

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

const char *CS_slabDesc( struct CS_SlabAllocator *allocation ) {
    struct CS_SlabAllocator *slab = (struct CS_SlabAllocator *)allocation;
    struct CS_StringBuilder *sb = CS_SB_create( 2048 );
    if( sb == NULL ) return NULL;
    pthread_mutex_lock(&slab->slabMutex);
    int totalSize = 0;
    int slabCount = 0;
    int totalFree = 0;
    int totalCapacity = 0;
    struct CS_SlabAllocator *current = slab;
    struct SlabAllocItem *currentItem = NULL;
    while( current ) {
        totalSize += (current->size * current->capacity) + sizeof(struct CS_SlabAllocator);
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
