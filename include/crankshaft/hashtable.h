#ifndef __crankshafthashtabledoth__
#define __crankshafthashtabledoth__
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

/********************************************************************
 *
 * Generic hashtable. Init function takes basic functions for each
 * operation (Hash the key, compare keys, key entry creation/removal
 * etc...) And a couple basic examples used within the main library
 * (string to malloc'd void* and UUID to malloc'd void*)
 *
 ********************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

struct CS_HashTableEntry {
    int32_t keyHash;
    const char keyPrefix[4];
    const void *fullKey;
    struct CS_HashTableEntry *nextInBucket;
    const void *value;
};

#define CS_HASHTABLE_FLAG_MUTEX         0x00000001
#define CS_HASHTABLE_FLAG_MALLOC        0x00000010
#define CS_HASHTABLE_FLAG_PEDANTIC      0x00010000
#define CS_HASHTABLE_FLAG_VERY_PEDANTIC 0x00030000

struct CS_HashTable {
    int32_t capacity;
    uint32_t flags;
    pthread_mutex_t hashTableMutex;
    int32_t (*keyHashFunction)( struct CS_HashTable *table, const void *key );
    int32_t (*keyCompareFunction)( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *rightKey, int32_t rightKeyHash );
    int32_t (*entryInitFunction)( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int32_t hash, const void *value );
    int32_t (*entryRemoveFunction)( struct CS_HashTable *table, struct CS_HashTableEntry *entry );
    int32_t (*cleanupFunction)( struct CS_HashTable *table );
    const char *(*keyToTempString)( struct CS_HashTable *table, const struct CS_HashTableEntry *entry );
    struct CS_HashTableEntry **entries;
    struct CS_SlabAllocator *hashTableEntrySlabAllocator;
    void *applicationSpecificData;
};

#define CS_HASHTABLE_ERROR ((const void *)(-1))

#define CS_HASHTABLE_ITER(__TABLE,__ITER) for(struct CS_HashTableEntry **SLOT##__ITER=((__TABLE)->entries),**END##__ITER=((__TABLE)->entries+(__TABLE)->capacity);SLOT##__ITER<END##__ITER;++SLOT##__ITER)for(struct CS_HashTableEntry *__ITER=*(SLOT##__ITER); __ITER; __ITER=__ITER->nextInBucket)
void CS_hashtableGrabMutex( struct CS_HashTable *table );
void CS_hashtableReleaseMutex( struct CS_HashTable *table );

struct CS_HashTable *CS_hashtableCreateCustom( int32_t capacity, uint32_t flags,
        void *applicationSpecificData,
        int32_t (*keyHashFunction)(struct CS_HashTable *table,const void *key),
        int32_t (*keyCompareFunction)(struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *rightKey, int32_t rightKeyHash),
        int32_t (*entryInitFunction)(struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int32_t hash, const void *value),
        int32_t (*entryRemoveFunction)(struct CS_HashTable *table, struct CS_HashTableEntry *entry),
        const char *(*keyToTempString)(struct CS_HashTable *table, const struct CS_HashTableEntry *entry),
        int32_t (*cleanupFunction)(struct CS_HashTable *table)
        );

void CS_hashtableFree( struct CS_HashTable *table );


int32_t CS_hashtableDefaultStringVoidEntryInit( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int32_t hash, const void *value );
int32_t CS_hashtableDefaultStringVoidEntryRemove( struct CS_HashTable *table, struct CS_HashTableEntry *entry );
int32_t CS_hashtableDefaultStringKeyHash( struct CS_HashTable *table, const void *key );
int32_t CS_hashtableDefaultStringKeyCompare( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *key, int32_t hash );
const char *CS_hashtableDefaultStringKeyToTempString( struct CS_HashTable *table, const struct CS_HashTableEntry *entry );

int32_t CS_hashtableDefaultUuidVoidEntryInit( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int32_t hash, const void *value );
int32_t CS_hashtableDefaultUuidVoidEntryRemove( struct CS_HashTable *table, struct CS_HashTableEntry *entry );
int32_t CS_hashtableDefaultUuidKeyHash( struct CS_HashTable *table, const void *key );
int32_t CS_hashtableDefaultUuidKeyCompare( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *key, int32_t hash );
int32_t CS_hashtableDefaultUuidCleanup( struct CS_HashTable *table );
const char *CS_hahstableDefaultUuidKeyToTempString( struct CS_HashTable *table, const struct CS_HashTableEntry *entry );

#define CS_HASHTABLE_STRING_VOID(__CAPACITY,__FLAGS) CS_hashtableCreateCustom(__CAPACITY,__FLAGS,NULL,CS_hashtableDefaultStringKeyHash,CS_hashtableDefaultStringKeyCompare,CS_hashtableDefaultStringVoidEntryInit,CS_hashtableDefaultStringVoidEntryRemove,CS_hashtableDefaultStringKeyToTempString,NULL)
#define CS_HASHTABLE_UUID_VOID(__CAPACITY,__FLAGS,__KEY_SLAB_ALLOC) CS_hashtableCreateCustom(__CAPACITY,__FLAGS,__KEY_SLAB_ALLOC,CS_hashtableDefaultUuidKeyHash,CS_hashtableDefaultUuidKeyCompare,CS_hashtableDefaultUuidVoidEntryInit,CS_hashtableDefaultUuidVoidEntryRemove,CS_hahstableDefaultUuidKeyToTempString,CS_hashtableDefaultUuidCleanup)

struct CS_HashTable *CS_hashtableResize( struct CS_HashTable *hashTable, int32_t newCapacity );

//For CS_hashtablePutMaybe's 'maybe' function. You return one of these.
enum CS_HASHTTABLE_MAYBE {
    CS_HASHTABLE_MAYBE_PUT_RETURN_NEW,
    CS_HASHTABLE_MAYBE_PUT_RETURN_OLD,
    CS_HASHTABLE_MAYBE_PUT_RETURN_NULL,
    CS_HASHTABLE_MAYBE_RETURN_ERROR,
    CS_HASHTABLE_MAYBE_RETURN_NEW,
    CS_HASHTABLE_MAYBE_RETURN_OLD,
    CS_HASHTABLE_MAYBE_RETURN_NULL
};
bool CS_hashtableHasKey( struct CS_HashTable *table, const void *key );
const void *CS_hashtablePut( struct CS_HashTable *table, const void *key, const void *value );
const void *CS_hashtablePutMaybe( struct CS_HashTable *table, const void *key, const void *value, void *context, int32_t (*maybe)(void *context, const void *oldValue) );
const void *CS_hashtableRemove( struct CS_HashTable *table, const void *key );
const void *CS_hashtableGet( struct CS_HashTable *table, const void *key );
struct CS_List *CS_hashtableGetKeys( struct CS_HashTable *table );

#ifdef __cplusplus
}
#endif
#endif
