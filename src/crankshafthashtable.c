#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftslaballoc.h"
#include "crankshaftuuid.h"
#include "crankshafthash.h"

#include "crankshafthashtable.h"

#define GRAB_MUTEX() if(table->flags&CS_HASHTABLE_FLAG_MUTEX){pthread_mutex_lock(&table->hashTableMutex);}
#define RELEASE_MUTEX() if(table->flags&CS_HASHTABLE_FLAG_MUTEX){pthread_mutex_unlock(&table->hashTableMutex);}

struct CS_HashTable *CS_hashtableCreateCustom( int capacity, unsigned int flags,
        void *applicationSpecificData,
        int (*keyHashFunction)(struct CS_HashTable *table,const void *key),
        int (*keyCompareFunction)(struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *rightKey, int rightKeyHash),
        int (*entryInitFunction)(struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value),
        int (*entryRemoveFunction)(struct CS_HashTable *table, struct CS_HashTableEntry *entry),
        int (*cleanupFunction)(struct CS_HashTable *table) ) {
    struct CS_HashTable *returnValue = CS_allocB( flags&CS_HASHTABLE_FLAG_MALLOC,sizeof( struct CS_HashTable ) );
    if( returnValue ) {
        memset( returnValue, 0, sizeof(struct CS_HashTable) );
        returnValue->capacity = capacity;
        returnValue->flags = flags;
        returnValue->applicationSpecificData = applicationSpecificData;
        if( flags & CS_HASHTABLE_FLAG_MUTEX ) {
            if( pthread_mutex_init( &returnValue->hashTableMutex, NULL ) ) goto FAIL;
        }
        returnValue->hashTableEntrySlabAllocator = (flags&CS_HASHTABLE_FLAG_MALLOC)?CS_slabInitMalloc("HashTable Malloc", sizeof(struct CS_HashTableEntry), capacity, sizeof(void*)):CS_slabInit("HashTable", sizeof(struct CS_HashTableEntry), capacity, sizeof(void *));
        if( returnValue->hashTableEntrySlabAllocator == NULL ) goto FAIL;
        returnValue->entries = CS_allocB( flags&CS_HASHTABLE_FLAG_MALLOC, capacity * sizeof(struct CS_HashTableEntry*) );
        memset( returnValue->entries, 0, capacity * sizeof(struct CS_HashTableEntry*) );
        if( returnValue->entries == NULL ) {
            goto FAIL;
        }
        returnValue->keyHashFunction = keyHashFunction;
        returnValue->keyCompareFunction = keyCompareFunction;
        returnValue->entryInitFunction = entryInitFunction;
        returnValue->entryRemoveFunction = entryRemoveFunction;
        returnValue->cleanupFunction = cleanupFunction;
    }
    return returnValue;

FAIL:
    if( returnValue ) {
        if( returnValue->hashTableEntrySlabAllocator ) CS_slabFree( returnValue->hashTableEntrySlabAllocator );
        if( returnValue->entries ) CS_freeB( flags&CS_HASHTABLE_FLAG_MALLOC, returnValue->entries );
        CS_freeB( flags&CS_HASHTABLE_FLAG_MALLOC, returnValue );
    }
    return NULL;
}

void CS_hashtableFree( struct CS_HashTable *table ) {
    GRAB_MUTEX();
    CS_HASHTABLE_ITER(table,entry) {
        table->entryRemoveFunction( table, entry );
    }
    if( table->cleanupFunction ) {
        table->cleanupFunction( table );
    }
    CS_slabFree( table->hashTableEntrySlabAllocator );
    CS_freeB( table->flags&CS_HASHTABLE_FLAG_MALLOC, table->entries );
    RELEASE_MUTEX();
    CS_free( table );
}

struct CS_HashTableEntry *privateFindEntryForKey( struct CS_HashTable *table, const void *key, int hash, struct CS_HashTableEntry **previousOut ) {
    struct CS_HashTableEntry *previous = NULL;
    struct CS_HashTableEntry **slot = table->entries + abs(hash % table->capacity);
    struct CS_HashTableEntry *current = *slot;

    while( current ) {
        if( table->keyCompareFunction(table,current,key,hash) == 0 ) {
            break;
        }
        previous = current;
        current = current->nextInBucket;
    }
    if( previousOut ) {
        *previousOut = previous;
    }
    return current;
}

#define INSERT_INTO_ARRAY(__ENTRIES,__CAPACITY,__ENTRY){struct CS_HashTableEntry **__SLOT=(__ENTRIES)+abs(((__ENTRY)->keyHash)%(__CAPACITY));(__ENTRY)->nextInBucket=*__SLOT;*__SLOT=(__ENTRY);}

struct CS_HashTableEntry *privateInsertEntryForKey( struct CS_HashTable *table, const void *key, int hash, const void *value ) {
    struct CS_HashTableEntry *newEntry = CS_slabTake( table->hashTableEntrySlabAllocator );
    if( newEntry ) {
        memset( newEntry, 0, sizeof( struct CS_HashTableEntry ) );
        table->entryInitFunction( table, newEntry, key, hash, value );
        INSERT_INTO_ARRAY(table->entries,table->capacity,newEntry);
    }
    return newEntry;
}

struct CS_HashTable *CS_hashtableResize( struct CS_HashTable *table, int newCapacity ) {
    struct CS_HashTableEntry **newEntries = CS_allocB( table->flags&CS_HASHTABLE_FLAG_MALLOC, newCapacity * sizeof(struct CS_HashTableEntry *) );
    memset( newEntries, 0, newCapacity * sizeof(struct CS_HashTableEntry *) );
    GRAB_MUTEX();
    for( int i = 0; i < table->capacity; ++i ) {
        struct CS_HashTableEntry *current = table->entries[ i ];
        while( current ) {
            struct CS_HashTableEntry *next = current->nextInBucket;
            INSERT_INTO_ARRAY(newEntries,newCapacity,current);
            current = next;
        }
    }
    struct CS_HashTableEntry **oldEntries = table->entries;
    table->entries = newEntries;
    table->capacity = newCapacity;
    CS_freeB(table->flags&CS_HASHTABLE_FLAG_MALLOC,oldEntries);
    RELEASE_MUTEX();
    return NULL;
}

bool CS_hashTableHasKey( struct CS_HashTable *table, const void *key) {
    return CS_hashtableGet( table, key ) != NULL;
}

const void *CS_hashtablePut( struct CS_HashTable *table, const void *key, const void *value ) {
    int hash = table->keyHashFunction( table, key );
    const void *returnValue = NULL;
    GRAB_MUTEX();
    struct CS_HashTableEntry *entry = privateFindEntryForKey( table, key, hash, NULL );
    if( entry ) {
        returnValue = entry->value;
        entry->value = value;
    } else {
        if(privateInsertEntryForKey( table, key, hash, value ) == NULL) returnValue = CS_HASHTABLE_ERROR;
    }
    RELEASE_MUTEX();
    return returnValue;
}

const void *CS_hashtableRemove( struct CS_HashTable *table, const void *key ) {
    int hash = table->keyHashFunction( table, key );
    const void *returnValue = NULL;
    GRAB_MUTEX();
    struct CS_HashTableEntry *previous = NULL;
    struct CS_HashTableEntry *entry = privateFindEntryForKey( table, key, hash, &previous );
    if( entry ) {
        returnValue = entry->value;
        if( previous ) {
            previous->nextInBucket = entry->nextInBucket;
        } else {
            struct CS_HashTableEntry **slot = table->entries + abs(hash % table->capacity);
            *slot = entry->nextInBucket;
        }
        entry->nextInBucket = NULL;
        table->entryRemoveFunction( table, entry );
    } else {
        returnValue = CS_HASHTABLE_ERROR;
    }
    RELEASE_MUTEX();
    return returnValue;
}

const void *CS_hashtableGet( struct CS_HashTable *table, const void *key ) {
    int hash = table->keyHashFunction( table, key );
    const void *returnValue;
    GRAB_MUTEX();

    struct CS_HashTableEntry *entry = privateFindEntryForKey( table, key, hash, NULL );
    if( entry ) {
        returnValue = entry->value;
    } else {
        returnValue = CS_HASHTABLE_ERROR;
    }

    RELEASE_MUTEX();
    return returnValue;
}


int CS_hashtableDefaultStringVoidEntryInit( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value ) {
    int nLen = strlen( key );
    void *newKey = CS_allocB( table->flags&CS_HASHTABLE_FLAG_MALLOC, nLen + 1 );
    if( !newKey ) return -1;
    strcpy( newKey, key );
    strncpy( (char*)entry->keyPrefix, key, sizeof( entry->keyPrefix ) );
    entry->keyHash = hash;
    entry->fullKey = newKey;
    entry->value = value;
    return 0;
}

int CS_hashtableDefaultStringKeyCompare( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *key, int hash ) {
    if( entry->keyHash == hash ) {
        if( table->flags & CS_HASHTABLE_FLAG_PEDANTIC ) {
            if( strncmp( entry->keyPrefix, key, sizeof( entry->keyPrefix ) ) ) {
                CS_LOG_WARN("We had a key collision, caught by prefix check. %s vs %s", key, entry->fullKey );
                return 1;
            }
            if( table->flags & CS_HASHTABLE_FLAG_VERY_PEDANTIC ) {
                if( strcmp( entry->fullKey, key ) ) {
                    CS_LOG_WARN("We had a key collision, caught by pedantic check. %s vs %s", key, entry->fullKey);
                    return 1;
                }
            }
        }
        return 0;
    }
    return 1;
}

int CS_hashtableDefaultStringVoidEntryRemove( struct CS_HashTable *table, struct CS_HashTableEntry *entry ) {
    if( entry->fullKey ) CS_freeB( table->flags&CS_HASHTABLE_FLAG_MALLOC, (void*)entry->fullKey );
    return 0;
}

int CS_hashtableDefaultStringKeyHash( struct CS_HashTable *table, const void *key ) {
    return CS_hash( key );
}

int CS_hashtableDefaultUuidVoidEntryInit( struct CS_HashTable *table, struct CS_HashTableEntry *entry, const void *key, int hash, const void *value ) {
    const struct CS_UUID *keyUuid = (const struct CS_UUID *)key;
    struct CS_UUID *uuid = CS_slabTake(table->applicationSpecificData);
    CS_uuidCopy( uuid, keyUuid );
    *(int*)entry->keyPrefix = *(int*)keyUuid->uuid;
    entry->keyHash = hash;
    entry->fullKey = uuid;
    entry->value = value;
    return 0;
}

int CS_hashtableDefaultUuidVoidEntryRemove( struct CS_HashTable *table, struct CS_HashTableEntry *entry ) {
    if( CS_slabReturn( table->applicationSpecificData, (void*)entry->fullKey) ) return -1;
    return 0;
}

int CS_hashtableDefaultUuidKeyHash( struct CS_HashTable *table, const void *key ) {
    return CS_hashBin(key, sizeof(struct CS_UUID));
}

int CS_hashtableDefaultUuidKeyCompare( struct CS_HashTable *table, const struct CS_HashTableEntry *entry, const void *key, int hash ) {
    if( entry->keyHash == hash ) {
        if( table->flags & CS_HASHTABLE_FLAG_PEDANTIC ) {
            if( memcmp( entry->keyPrefix, key, sizeof( entry->keyPrefix ) ) ) {
                CS_LOG_WARN("We had a key collision, caught by prefix check." );
                return 1;
            }
            if( table->flags & CS_HASHTABLE_FLAG_VERY_PEDANTIC ) {
                if( memcmp( entry->fullKey, key, sizeof( struct CS_UUID ) ) ) {
                    CS_LOG_WARN("We had a key collision, caught by pedantic check.");
                    return 1;
                }
            }
        }
        return 0;
    }
    return 1;
}

int CS_hashtableDefaultUuidCleanup( struct CS_HashTable *table ) {
    CS_slabFree( table->applicationSpecificData );
    return 0;
}

void CS_hashtableGrabMutex( struct CS_HashTable *table ) {
    GRAB_MUTEX();
}
void CS_hashtableReleaseMutex( struct CS_HashTable *table ) {
    RELEASE_MUTEX();
}
