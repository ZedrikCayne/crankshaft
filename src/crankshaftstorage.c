#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

//#include "sqlite3.h"

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"

#include "crankshaftslaballoc.h"
#include "crankshafthashtable.h"
#include "crankshaftstorage.h"
#include "crankshaftstring.h"
#include "crankshaftstack.h"

//Engine define storage bits.
struct CS_Storage     *privateSqliteOpen(struct CS_Storage *storage);
bool                   privateSqliteClose(struct CS_Storage *storage);
unsigned int           privateSqliteGetCas(const struct CS_Storage *storage, const char *key);
struct CS_StorageItem *privateSqliteGet(const struct CS_Storage *storage, const char *key);
struct CS_StorageItem *privateSqlitePut(const struct CS_Storage *storage, struct CS_StorageItem *item );
struct CS_StorageItem *privateSqliteRemove(const struct CS_Storage *storage, const char *key);

struct CS_Storage     *privateHashtableOpen(struct CS_Storage *storage);
bool                   privateHashtableClose(struct CS_Storage *storage);
unsigned int           privateHashtableGetCas(const struct CS_Storage *storage, const char *key);
struct CS_StorageItem *privateHashtableGet(const struct CS_Storage *storage, const char *key);
struct CS_StorageItem *privateHashtablePut(const struct CS_Storage *storage, struct CS_StorageItem *item );
struct CS_StorageItem *privateHashtableRemove(const struct CS_Storage *storage, const char *key);

static const struct CS_StorageBackend _CS_STORAGE_BACKEND_SQLITE = {
    "SQLITE",
    privateSqliteOpen,
    privateSqliteClose,
    privateSqliteGet,
    privateSqliteGetCas,
    privateSqlitePut,
    privateSqliteRemove
};
static const struct CS_StorageBackend _CS_STORAGE_BACKEND_HASHTABLE = {
    "HASHTABLE",
    privateHashtableOpen,
    privateHashtableClose,
    privateHashtableGet,
    privateHashtableGetCas,
    privateHashtablePut,
    privateHashtableRemove
};

const struct CS_StorageBackend *CS_STORAGE_BACKEND_SQLITE = &_CS_STORAGE_BACKEND_SQLITE;
const struct CS_StorageBackend *CS_STORAGE_BACKEND_HASHTABLE = &_CS_STORAGE_BACKEND_HASHTABLE;

static void *storageItemSlabAllocator = NULL;
pthread_mutex_t storageItemMutex = PTHREAD_MUTEX_INITIALIZER;

struct CS_StorageItem *privateGetStorageItem(void) {
    if( storageItemSlabAllocator  == NULL ) {
        pthread_mutex_lock( &storageItemMutex );
        if( !storageItemSlabAllocator ) storageItemSlabAllocator = CS_slabInit( "CS_ReturnItems", sizeof(struct CS_StorageItem), 200, CRANKSHAFT_MIN_ALIGNMENT );
        pthread_mutex_unlock( &storageItemMutex );
        if( !storageItemSlabAllocator ) return NULL;
    }
    struct CS_StorageItem *returnValue = CS_slabTake( storageItemSlabAllocator );
    if( returnValue ) {
        returnValue->cas = 0;
        returnValue->size = 0;
        returnValue->key = NULL;
        returnValue->value = NULL;
    }
    return returnValue;
}

void privateReturnStorageItem(struct CS_StorageItem *item) {
    if( item->key ) CS_stringFree( item->key );
    item->key = NULL;
    if( item->value ) CS_free( item->value );
    item->value = NULL;
    item->cas = 0;
    item->size = 0;
    if( storageItemSlabAllocator ) CS_slabReturn( storageItemSlabAllocator, item );
}

struct CS_StorageItem *privateGetStorageItemWithCopyData( const char *key, int cas, int size, const void *data ) {
    struct CS_StorageItem *item = privateGetStorageItem();
    if( item ) {
        item->cas = cas;
        item->size = size;
        item->key = CS_stringCopy(key);
        if( key == NULL ) {
            privateReturnStorageItem(item);
            return NULL;
        }
        item->value = CS_allocDuplicate(data,size);
        if( item->value == NULL ) {
            privateReturnStorageItem(item);
            return NULL;
        }
    }
    return item;
}

struct CS_StorageItem *privateDeepCopyStorageItem( const struct CS_StorageItem *item ) {
    return privateGetStorageItemWithCopyData( item->key, item->cas, item->size, item->value );
}

static struct CS_HashTable *storageHashTable = NULL;
pthread_mutex_t storageHashTableMutex = PTHREAD_MUTEX_INITIALIZER;

struct CS_HashTable *getStorageHashTable( void ) {
    if( storageHashTable == NULL ) {
        pthread_mutex_lock( &storageHashTableMutex );
        storageHashTable = CS_HASHTABLE_STRING_VOID( 50, CS_HASHTABLE_FLAG_MUTEX|CS_HASHTABLE_FLAG_VERY_PEDANTIC );
        pthread_mutex_unlock( &storageHashTableMutex );
    }
    return storageHashTable;
}

void CS_storageTeardown(void) {
    pthread_mutex_lock( &storageHashTableMutex );
    struct CS_Stack *stack = CS_stackAllocPointer( 50 );
    CS_HASHTABLE_ITER(storageHashTable,iter) {
        CS_stackPushPointer( stack, iter->value );
    };
    void *aVal;
    while( (aVal = CS_stackPopPointer( stack )) != NULL ) {
        CS_storageClose((struct CS_Storage *)aVal);
    }
    CS_stackFree( stack );
    CS_hashtableFree( storageHashTable );
    storageHashTable = NULL;
    pthread_mutex_unlock( &storageHashTableMutex );
    pthread_mutex_lock( &storageItemMutex );
    CS_slabFree( storageItemSlabAllocator );
    storageItemSlabAllocator = NULL;
    pthread_mutex_unlock( &storageItemMutex );
}

const struct CS_Storage *CS_storageGetStorage( const char *storageName ) {
    struct CS_HashTable *storageTable = getStorageHashTable();
    if( !storageTable ) return NULL;
    const void *voidStorage = CS_hashtableGet( storageTable, storageName );
    if( voidStorage == CS_HASHTABLE_ERROR ) voidStorage = NULL;
    return (const struct CS_Storage *)voidStorage;
}

void privateFreeStorageStruct( const struct CS_Storage *storage ) {
    CS_stringFree((void*)storage->name);
    CS_stringFree((void*)storage->config);
    CS_free((void*)storage);
}

struct CS_Storage *privateInitStorageStruct( const char *storageName, const char *config, const struct CS_StorageBackend *backend ) {
    struct CS_Storage *storage = (struct CS_Storage *)CS_alloc(sizeof(struct CS_Storage));
    if( storage ) {
        storage->name = CS_stringCopy( storageName );
        storage->backend = backend;
        storage->config = CS_stringCopy( config );
        storage->storageData = NULL;
        if( (storage->name == NULL) || 
            (config&&!storage->config) ||
            (pthread_mutex_init(&storage->storageMutex,NULL) < 0) ) {
            privateFreeStorageStruct( storage );
            storage = NULL;
        }
    }
    return storage;
}

const struct CS_Storage *CS_storageOpen(const char *storageName, const char *config, const struct CS_StorageBackend *backend) {
    struct CS_HashTable *storageTable = getStorageHashTable();
    if( !storageTable ) return NULL;
    pthread_mutex_lock(&storageHashTableMutex);
    const void *voidStorage = CS_hashtableGet( storageTable, storageName );
    if( voidStorage != CS_HASHTABLE_ERROR ) {
        const struct CS_Storage *storage = (const struct CS_Storage *)voidStorage;
        CS_LOG_ERROR("Trying to open a storage %s when one already exists with backend %s.",
                storageName, storage->backend->name );
        goto UNLOCK_MUTEX;
    }
    struct CS_Storage *storage = privateInitStorageStruct( storageName, config, backend );
    if( !storage ) {
        CS_LOG_ERROR("Failed to allocate a storage structure.");
        goto FREE_STORAGE_STRUCT;
    }
    struct CS_Storage *initializedStorage = backend->open( storage );
    if( initializedStorage == NULL ) {
        privateFreeStorageStruct( storage );
        CS_LOG_ERROR("Failed to open a storage.");
        goto FREE_STORAGE_STRUCT;
    }
    const void *storedStorage = CS_hashtablePut( storageTable, storageName, storage );
    if( storedStorage != NULL ) {
        CS_LOG_ERROR("Between checking to see if we've got a storage to trying to open a new storage, we've somehow acquired a storage with the same name...putting it back and closing the new one. This should not be possible.");
        CS_hashtablePut( storageTable, storageName, storedStorage );
        goto CLOSE_STORAGE;
    }
    pthread_mutex_unlock(&storageHashTableMutex);
    return storage;
CLOSE_STORAGE:
    backend->close( storage );
FREE_STORAGE_STRUCT:
    privateFreeStorageStruct( storage );
UNLOCK_MUTEX:
    pthread_mutex_unlock(&storageHashTableMutex);
    return NULL;
}

bool CS_storageClose(const struct CS_Storage *closeMe) {
    if( storageHashTable == NULL ) {
        CS_LOG_ERROR("Trying to close a storage when the storage system is toast.");
    } else {
        pthread_mutex_lock(&storageHashTableMutex);
        struct CS_Storage *removed = (struct CS_Storage *)CS_hashtableRemove( storageHashTable, closeMe->name );
        if( removed != closeMe ) {
            CS_hashtablePut( storageHashTable, closeMe->name, removed );
            CS_LOG_ERROR("We've asked to close a storage that wasn't under our control. What?");
        }
        pthread_mutex_unlock(&storageHashTableMutex);
    }
    bool returnValue = closeMe->backend->close( (struct CS_Storage *)closeMe );
    privateFreeStorageStruct( closeMe );
    return returnValue;
}

struct CS_StorageItem *CS_storageGet(const struct CS_Storage *storage, const char *key ) {
    return storage->backend->get( storage, key );
}

struct CS_StorageItem *CS_storageRemove( const struct CS_Storage *storage, const char *key ) {
    pthread_mutex_lock((pthread_mutex_t*)&storage->storageMutex);
    struct CS_StorageItem *returnValue = storage->backend->remove( storage, key );
    pthread_mutex_unlock((pthread_mutex_t*)&storage->storageMutex);
    return returnValue;
}

struct CS_StorageItem *CS_storageUpdate( const struct CS_Storage *storage, struct CS_StorageItem *update ) {
    pthread_mutex_lock((pthread_mutex_t*)&storage->storageMutex);
    struct CS_StorageItem *returnValue = storage->backend->put( storage, update );
    pthread_mutex_unlock((pthread_mutex_t*)&storage->storageMutex);
    return returnValue;
}

struct CS_StorageItem *CS_storagePut( const struct CS_Storage *storage, const char *key, const void *value, int size ) {
    pthread_mutex_lock((pthread_mutex_t*)&storage->storageMutex);
    struct CS_StorageItem *newItem = privateGetStorageItemWithCopyData( key, 0, size, value);
    if( newItem == NULL ) {
        CS_LOG_ERROR("Failed to create a new storage item.");
        goto UNLOCK_MUTEX;
    }
    struct CS_StorageItem *returnValue = storage->backend->put( storage, newItem );
    pthread_mutex_unlock((pthread_mutex_t*)&storage->storageMutex);
    return returnValue;
UNLOCK_MUTEX:
    pthread_mutex_unlock((pthread_mutex_t*)&storage->storageMutex);
    return NULL;
}

void CS_storageReturnItem( struct CS_StorageItem *item ) {
    if( !item ) return;
    privateReturnStorageItem(item);
}

struct CS_Storage *privateSqliteOpen(struct CS_Storage *storage) {
    return NULL;
}

bool privateSqliteClose(struct CS_Storage *storage) {
    return false;
}
struct CS_StorageItem *privateSqliteGet(const struct CS_Storage *storage, const char *key) {
    return NULL;
}
unsigned int privateSqliteGetCas(const struct CS_Storage *storage, const char *key ) {
    return 0;
}
struct CS_StorageItem *privateSqlitePut(const struct CS_Storage *storage, struct CS_StorageItem *item ) {
    return NULL;
}
struct CS_StorageItem *privateSqliteRemove(const struct CS_Storage *storage, const char *key) {
    return NULL;
}

struct CS_Storage *privateHashtableOpen(struct CS_Storage *storage) {
    long initialCapacity = 50;
    if( storage->config ) {
        char *endPtr;
        initialCapacity = strtol( storage->config, &endPtr, 10 );
        if( endPtr == storage->config ) {
            return NULL;
        }
        if( ((initialCapacity == INT_MIN) || (initialCapacity == LONG_MAX)) 
            && (errno == ERANGE) ) {
            return NULL;
        }
        if( initialCapacity == 0 ) {
            CS_LOG_ERROR( "Cannot make a 0 capacity storage." );
            return NULL;
        }
    }
    storage->storageData = (void*)CS_HASHTABLE_STRING_VOID(initialCapacity,CS_HASHTABLE_FLAG_MUTEX|CS_HASHTABLE_FLAG_VERY_PEDANTIC);
    return storage;
}

bool privateHashtableClose(struct CS_Storage *storage) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    //The hashtable has copies of all the storage items.
    CS_HASHTABLE_ITER( table, item ) {
        if( item->value ) privateReturnStorageItem( (struct CS_StorageItem*)item->value );
        item->value = NULL;
    }
    CS_hashtableFree( table );
    storage->storageData = NULL;
    return false;
}

struct CS_StorageItem *privateHashtableGet(const struct CS_Storage *storage, const char *key) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    const void *maybe = CS_hashtableGet( table, key );
    if( maybe == CS_HASHTABLE_ERROR ) return NULL;
    return privateDeepCopyStorageItem( (const struct CS_StorageItem *)maybe );
}

unsigned int privateHashtableGetCas(const struct CS_Storage *storage, const char *key) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    const void *maybe = CS_hashtableGet( table, key );
    if( maybe == CS_HASHTABLE_ERROR ) return 0;
    return ((const struct CS_StorageItem *)maybe)->cas;
}

struct CS_StorageItem *privateHashtablePut(const struct CS_Storage *storage, struct CS_StorageItem *item ) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    const void *maybe = CS_hashtableGet(table, item->key);
    if( item->cas == 0 ) {
        if( maybe != CS_HASHTABLE_ERROR ) return NULL;
    } else {
        if( maybe == CS_HASHTABLE_ERROR ) return NULL;
        struct CS_StorageItem *currentItem = (struct CS_StorageItem *)maybe;
        if( currentItem->cas != item->cas ) return privateDeepCopyStorageItem(currentItem);
    }
    item->cas++;
    struct CS_StorageItem *old = (struct CS_StorageItem *)CS_hashtablePut(table, item->key, privateDeepCopyStorageItem(item));
    if( old && old != CS_HASHTABLE_ERROR ) privateReturnStorageItem(old);
    return item;
}
struct CS_StorageItem *privateHashtableRemove(const struct CS_Storage *storage, const char *key) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    struct CS_StorageItem *oldItem = (struct CS_StorageItem *)CS_hashtableRemove( table, key );
    return oldItem==CS_HASHTABLE_ERROR?NULL:oldItem;
}
