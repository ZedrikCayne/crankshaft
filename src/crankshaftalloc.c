#include <stdlib.h>
#include "crankshaftalloc.h"
#include "crankshaftrandom.h"


#ifdef CS_ALLOC_TRACKING
#include <pthread.h>
#include <string.h>
#include "crankshaftslaballoc.h"
#include "crankshafthashtable.h"
#include "crankshaftlogger.h"

//Hash table functions for storing the extra data for tracking
struct CS_AllocInfo {
    const char *file;
    int line;
    unsigned long size;
};

static pthread_mutex_t trackingSystemMutex = PTHREAD_MUTEX_INITIALIZER;
static void *trackingSystemSlabAllocator = NULL;
static struct CS_HashTable *trackingSystemHashTable = NULL;
static unsigned int trackingFlags = 0;

#define GRAB_MUTEX() pthread_mutex_lock(&trackingSystemMutex)
#define RELEASE_MUTEX() pthread_mutex_unlock(&trackingSystemMutex)

static unsigned long currentlyAllocated = 0;
static unsigned long maxAllocated = 0;

//Hopefully just storing the lowest 32 bits should be unique enough hash.
static int keyHash(struct CS_HashTable *table,const void *key) {
    return (int)(((unsigned long long)key)&0x0000FFFFl);
}
//Keys are just void *'s... we can 100% compare the keys and be fully correct.
static int keyCompare(struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *rightKey, int rightKeyHash) {
    return entry->fullKey != rightKey;
}
//We're not going to use the key prefix to try and make lookups shorter...so no need to set them.
static int entryInit(struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value) {
    entry->fullKey = key;
    entry->keyHash = hash;
    entry->value = value;
    return 0;
}
static int entryRemove(struct CS_HashTable *table, struct CS_HashTableEntry *entry) {
    return 0;
}
static int cleanup(struct CS_HashTable *table) {
    return 0;
}


#define TOP_COUNT 10
static struct CS_AllocInfo TOP[TOP_COUNT];

int CS_allocSystemTracker(int concurrentTrackingSlots, unsigned int flags) {
    if( trackingSystemSlabAllocator ) return 1;
    trackingFlags = flags;

    trackingSystemHashTable = CS_hashtableCreateCustom( concurrentTrackingSlots, CS_HASHTABLE_FLAG_VERY_PEDANTIC|CS_HASHTABLE_FLAG_MALLOC, NULL, keyHash, keyCompare, entryInit, entryRemove, cleanup );

    if( trackingSystemHashTable == NULL ) return 1;
    memset( TOP, 0, sizeof(TOP) );
    trackingSystemSlabAllocator = CS_slabInitMalloc( "alloc track", sizeof(struct CS_AllocInfo), concurrentTrackingSlots, sizeof(void *) );
    if( trackingSystemSlabAllocator == NULL ) {
        CS_hashtableFree( trackingSystemHashTable );
        trackingSystemHashTable = NULL;
        return 1;
    }

    return 0;
}

int CS_allocSystemTrackerKill() {
    if( !trackingSystemSlabAllocator ) return 1;
    void *oldAllocator = trackingSystemSlabAllocator;
    trackingSystemSlabAllocator = NULL;
    CS_slabFree( oldAllocator );
    CS_hashtableFree( trackingSystemHashTable );
    trackingSystemHashTable = NULL;
    trackingSystemSlabAllocator = NULL;
    return 0;
}

int CS_allocSystemReport() {
    CS_LOG_LOUD("Tracking report:");
    CS_LOG_LOUD("Current %d", currentlyAllocated);
    CS_LOG_LOUD("Max %d", maxAllocated);
    CS_LOG_LOUD("Top Allocations:");
    for( int i = 0; i < TOP_COUNT; ++i ) {
        CS_LOG_LOUD("%-25d %s line %d", TOP[i].size, TOP[i].file, TOP[i].line);
    }
    if( !trackingSystemSlabAllocator ) return 1;
    
    return 0;
}

static bool track(const void *pointer, int size, const char *file, int line) {
    struct CS_AllocInfo *newInfo = CS_slabTake( trackingSystemSlabAllocator );
    if( newInfo == NULL ) return true;
    currentlyAllocated += size;
    if( currentlyAllocated > maxAllocated ) maxAllocated = currentlyAllocated;
    newInfo->file = file;
    newInfo->line = line;
    newInfo->size = size;
    for( int i = 0; i < TOP_COUNT; ++i ) {
        if( size > TOP[i].size ) {
            for( int j = TOP_COUNT - 1; j > i; --j ) {
                TOP[ j ].file = TOP[ j - 1 ].file;
                TOP[ j ].line = TOP[ j - 1 ].line;
                TOP[ j ].size = TOP[ j - 1 ].size;
            }
            TOP[ i ].file = file;
            TOP[ i ].line = line;
            TOP[ i ].size = size;
            break;
        }
    }
    struct CS_AllocInfo *old = (struct CS_AllocInfo *)CS_hashtablePut( trackingSystemHashTable, pointer, newInfo );
    if( old ) {
        CS_LOG_ERROR("We're getting asked to track %p at %s line %d twice...which means it got free'd not through CS_free", pointer, file, line );
        if( CS_slabReturn( trackingSystemSlabAllocator, old ) ) {
            CS_LOG_ERROR("Somehow a tracking item was not allocated in our slab allocator..this is bad.");
        }
        return true;
    }
    return false;
}

static struct CS_AllocInfo *untrack(const void *pointer) {
    
    struct CS_AllocInfo *returnValue = (struct CS_AllocInfo *)CS_hashtableRemove( trackingSystemHashTable, pointer );
    if( returnValue ) {
        currentlyAllocated -= returnValue->size;
    }
    return returnValue;
}

static void returnInfo(struct CS_AllocInfo *old,const void *pointer) {
    if( !old ) {
        CS_LOG_WARN_IF( trackingFlags&CS_ALLOC_FLAG_LOG_ERRORS, "We were not tracking %p", pointer);
    } else {
        if( CS_slabReturn( trackingSystemSlabAllocator, old ) ) {
            CS_LOG_WARN_IF( trackingFlags&CS_ALLOC_FLAG_LOG_ERRORS, "Somehow a tracking item was not allocated in our slab allocator..this is bad.");
        }
    }
}

static void *trackMalloc(unsigned int size, const char *file, int line) {
    GRAB_MUTEX();
    void *returnValue = malloc(size);
    if( returnValue ) {
        track( returnValue, size, file, line );
    }
    RELEASE_MUTEX();
    return returnValue;
}

static void trackFree(const void *freeMe, const char *file, int line) {
    GRAB_MUTEX();
    struct CS_AllocInfo *old = untrack(freeMe);
    if( old && trackingFlags&CS_ALLOC_FLAG_WARN_LOCALITY ) {
        if( old->file != file ) {
            CS_LOG_WARN("Memory free'd from not the same file that alloc'd it. %s %d vs %s %d.", old->file, old->line, file, line);
        }
    }
    returnInfo( old, freeMe );
    free((void*)freeMe);
    RELEASE_MUTEX();
}

static void *trackRealloc(const void *reallocMe, unsigned int size, const char *file, int line) {
    GRAB_MUTEX();
    void *returnValue = realloc((void*)reallocMe, size);
    if( returnValue ) {
        struct CS_AllocInfo *old = untrack( reallocMe );
        if( old && trackingFlags&CS_ALLOC_FLAG_WARN_LOCALITY ) {
            if( old->file != file ) {
                CS_LOG_WARN("Memory realloc'd from not the same file that alloc'd it. %s %d vs %s %d.", old->file, old->line, file, line);
            }
        }
        returnInfo( old, reallocMe );
    }
    if( returnValue ) {
        track(returnValue, size, file, line);
    }
    RELEASE_MUTEX();
    return returnValue;
}

#define CS_MALLOC(X) trackingSystemSlabAllocator?trackMalloc(X,file,line):malloc(X)
#define CS_FREE(X) trackingSystemSlabAllocator?trackFree(X,file,line):free(X)
#define CS_REALLOC(X,Y) trackingSystemSlabAllocator?trackRealloc(X,Y,file,line):realloc(X,Y)
#else
#define CS_MALLOC(X) malloc(X)
#define CS_FREE(X) free(X)
#define CS_REALLOC(X,Y) realloc(X,Y)
#endif

#define MIN_FAIL_RATE 0
#define MAX_FAIL_RATE 100

#ifndef CS_ALLOC_USE_MALLOC
static int mallocFailRate = 0;
static int mallocFailSize = 0;

static struct CS_LCG_rand_state memoryRNG = {0xBADF00D};

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
    return CS_MALLOC(size);
}
void CS_free_detailled(void *freeMe,const char *file, int line) {
    return CS_FREE(freeMe);
}
void *CS_realloc_detailled(void *reallocMe,unsigned long size,const char *file, int line) {
    if( mallocFailSize > 0 ) if( size > mallocFailSize ) return NULL;
    if( mallocFailRate > 0 ) if( (CS_LCG_rand(&memoryRNG)%MAX_FAIL_RATE) < mallocFailRate ) return NULL;
    return CS_REALLOC(reallocMe,size);
}
#endif


