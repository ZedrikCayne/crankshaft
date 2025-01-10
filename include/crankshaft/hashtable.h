#ifndef __crankshafthashtabledoth__
#define __crankshafthashtabledoth__
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_HashTableEntry {
    int keyHash;
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
    int capacity;
    unsigned int flags;
    pthread_mutex_t hashTableMutex;
    int (*keyHashFunction)( struct CS_HashTable *table, const void *key );
    int (*keyCompareFunction)( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *rightKey, int rightKeyHash );
    int (*entryInitFunction)( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value );
    int (*entryRemoveFunction)( struct CS_HashTable *table, struct CS_HashTableEntry *entry );
    int (*cleanupFunction)( struct CS_HashTable *table );
    struct CS_HashTableEntry **entries;
    void *hashTableEntrySlabAllocator;
    void *applicationSpecificData;
};

#define CS_HASHTABLE_ERROR ((const void *)(-1))

#define CS_HASHTABLE_ITER(__TABLE,__ITER) for(struct CS_HashTableEntry **SLOT##__ITER=((__TABLE)->entries),**END##__ITER=((__TABLE)->entries+(__TABLE)->capacity);SLOT##__ITER<END##__ITER;++SLOT##__ITER)for(struct CS_HashTableEntry *__ITER=*(SLOT##__ITER); __ITER; __ITER=__ITER->nextInBucket)
void CS_hashtableGrabMutex( struct CS_HashTable *table );
void CS_hashtableReleaseMutex( struct CS_HashTable *table );

struct CS_HashTable *CS_hashtableCreateCustom( int capacity, unsigned int flags,
        void *applicationSpecificData,
        int (*keyHashFunction)(struct CS_HashTable *table,const void *key),
        int (*keyCompareFunction)(struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *rightKey, int rightKeyHash),
        int (*entryInitFunction)(struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value),
        int (*entryRemoveFunction)(struct CS_HashTable *table, struct CS_HashTableEntry *entry),
        int (*cleanupFunction)(struct CS_HashTable *table) );

void CS_hashtableFree( struct CS_HashTable *table );


int CS_hashtableDefaultStringVoidEntryInit( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value );
int CS_hashtableDefaultStringVoidEntryRemove( struct CS_HashTable *table, struct CS_HashTableEntry *entry );
int CS_hashtableDefaultStringKeyHash( struct CS_HashTable *table, const void *key );
int CS_hashtableDefaultStringKeyCompare( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *key, int hash );

int CS_hashtableDefaultUuidVoidEntryInit( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value );
int CS_hashtableDefaultUuidVoidEntryRemove( struct CS_HashTable *table, struct CS_HashTableEntry *entry );
int CS_hashtableDefaultUuidKeyHash( struct CS_HashTable *table, const void *key );
int CS_hashtableDefaultUuidKeyCompare( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *key, int hash );
int CS_hashtableDefaultUuidCleanup( struct CS_HashTable *table );

#define CS_HASHTABLE_STRING_VOID(__CAPACITY,__FLAGS) CS_hashtableCreateCustom(__CAPACITY,__FLAGS,NULL,CS_hashtableDefaultStringKeyHash,CS_hashtableDefaultStringKeyCompare,CS_hashtableDefaultStringVoidEntryInit,CS_hashtableDefaultStringVoidEntryRemove,NULL)
#define CS_HASHTABLE_UUID_VOID(__CAPACITY,__FLAGS,__KEY_SLAB_ALLOC) CS_hashtableCreateCustom(__CAPACITY,__FLAGS,__KEY_SLAB_ALLOC,CS_hashtableDefaultUuidKeyHash,CS_hashtableDefaultUuidKeyCompare,CS_hashtableDefaultUuidVoidEntryInit,CS_hashtableDefaultUuidVoidEntryRemove,CS_hashtableDefaultUuidCleanup)

struct CS_HashTable *CS_hashtableResize( struct CS_HashTable *hashTable, int newCapacity );

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
const void *CS_hashtablePutMaybe( struct CS_HashTable *table, const void *key, const void *value, void *context, int (*maybe)(void *context, const void *oldValue) );
const void *CS_hashtableRemove( struct CS_HashTable *table, const void *key );
const void *CS_hashtableGet( struct CS_HashTable *table, const void *key );

#ifdef __cplusplus
}
#endif
#endif
