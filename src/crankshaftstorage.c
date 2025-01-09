#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include <sqlite3.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/slaballoc.h>
#include <crankshaft/hashtable.h>
#include <crankshaft/storage.h>
#include <crankshaft/string.h>
#include <crankshaft/stack.h>

#include <crankshaft/tempbuff.h>
#include <crankshaft/stringbuilder.h>

//Engine define storage bits.
static struct CS_Storage     *privateSqliteOpen(struct CS_Storage *storage);
static bool                   privateSqliteClose(struct CS_Storage *storage);
static struct CS_StorageItem *privateSqliteGet(const struct CS_Storage *storage, const char *key);
static struct CS_StorageItem *privateSqlitePut(const struct CS_Storage *storage, struct CS_StorageItem *item );
static bool                   privateSqliteRemove(const struct CS_Storage *storage, const char *key);

static struct CS_Storage     *privateHashtableOpen(struct CS_Storage *storage);
static bool                   privateHashtableClose(struct CS_Storage *storage);
static struct CS_StorageItem *privateHashtableGet(const struct CS_Storage *storage, const char *key);
static struct CS_StorageItem *privateHashtablePut(const struct CS_Storage *storage, struct CS_StorageItem *item );
static bool                   privateHashtableRemove(const struct CS_Storage *storage, const char *key);

static const struct CS_StorageBackend _CS_STORAGE_BACKEND_SQLITE = {
    "SQLITE",
    privateSqliteOpen,
    privateSqliteClose,
    privateSqliteGet,
    privateSqlitePut,
    privateSqliteRemove
};
static const struct CS_StorageBackend _CS_STORAGE_BACKEND_HASHTABLE = {
    "HASHTABLE",
    privateHashtableOpen,
    privateHashtableClose,
    privateHashtableGet,
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
    if(storageItemSlabAllocator)CS_slabFree( storageItemSlabAllocator );
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

static void privateFreeStorageStruct( const struct CS_Storage *storage ) {
    CS_stringFree((void*)storage->name);
    CS_stringFree((void*)storage->config);
    CS_free((void*)storage);
}

static struct CS_Storage *privateInitStorageStruct( const char *storageName, const char *config, const struct CS_StorageBackend *backend ) {
    struct CS_Storage *storage = (struct CS_Storage *)CS_alloc(sizeof(struct CS_Storage));
    if( storage ) {
        storage->name = CS_stringCopy( storageName );
        storage->backend = backend;
        storage->config = CS_stringCopy( config );
        storage->storageData = NULL;
        if( (storage->name == NULL) || 
            (config&&!storage->config) ) {
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
    if( closeMe == NULL ) {
        CS_LOG_ERROR("Null parameter.");
        return true;
    }
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
    if( storage == NULL ) {
        CS_LOG_ERROR("CS_storageGet NULL storage.");
        return NULL;
    }
    return storage->backend->get( storage, key );
}

bool CS_storageRemove( const struct CS_Storage *storage, const char *key ) {
    if( storage == NULL ) {
        CS_LOG_ERROR("CS_storageRemove NULL storage.");
        return NULL;
    }
    bool returnValue = storage->backend->remove( storage, key );
    return returnValue;
}

struct CS_StorageItem *CS_storageUpdate( const struct CS_Storage *storage, struct CS_StorageItem *update ) {
    if( storage == NULL ) {
        CS_LOG_ERROR("CS_storageUpdate NULL storage.");
        return NULL;
    }
    struct CS_StorageItem *returnValue = storage->backend->put( storage, update );
    return returnValue;
}

struct CS_StorageItem *CS_storagePut( const struct CS_Storage *storage, const char *key, const void *value, int size, struct CS_StorageItem **outPresent ) {
    if( storage == NULL ) {
        CS_LOG_ERROR("CS_storagePut NULL storage.");
        return NULL;
    }
    struct CS_StorageItem *newItem = privateGetStorageItemWithCopyData( key, 0, size, value);
    if( newItem == NULL ) {
        CS_LOG_ERROR("Failed to create a new storage item.");
        return NULL;
    }
    struct CS_StorageItem *returnValue = storage->backend->put( storage, newItem );
    if( returnValue != newItem ) {
        privateReturnStorageItem( newItem );
        if( outPresent ) *outPresent = returnValue; else privateReturnStorageItem( returnValue );
        returnValue = NULL;
    } else {
        if( outPresent ) *outPresent = NULL;
    }
    return returnValue;
}

struct CS_StorageItem *CS_storageDuplicateItem( struct CS_StorageItem *item ) {
    return privateDeepCopyStorageItem( item );
}

struct CS_StorageItem *CS_storageCreateItemDataCopy( const char *key, unsigned long cas, unsigned int size, const void *data ) {
    return privateGetStorageItemWithCopyData( key, cas, size, data );
}

void CS_storageReturnItem( struct CS_StorageItem *item ) {
    if( !item ) return;
    privateReturnStorageItem(item);
}
struct CS_StorageItem *CS_storageItemChangeData( struct CS_StorageItem *item, unsigned int size, const void *data ) {
    void *newData = CS_allocDuplicate( data, size );
    if( !newData ) return NULL;
    void *oldData = item->value;
    CS_free( oldData );
    item->value = newData;
    item->size = size;
    return item;
}

/******************************************************************************************/
struct privateSqliteData {
    sqlite3 *connection;
    sqlite3_stmt *get;
    sqlite3_stmt *update;
    sqlite3_stmt *put;
    sqlite3_stmt *remove;
    const char *tableName;
    const char *dbFile;
    struct CS_StringBuilder *sb;
};

static const char *sqliteDefaultTableName = "default_storage";
static const char *sqliteDefaultDbFile = "storage/default_storage.sqlite";

#define IF_DO_NULL(_X,_DO) if(_X)_DO(_X);_X=NULL
static void privateFreeSqlite( struct privateSqliteData *sqliteData ) {
    if( sqliteData ) {
        IF_DO_NULL(sqliteData->put,sqlite3_finalize);
        IF_DO_NULL(sqliteData->get,sqlite3_finalize);
        IF_DO_NULL(sqliteData->update,sqlite3_finalize);
        IF_DO_NULL(sqliteData->remove,sqlite3_finalize);
        IF_DO_NULL(sqliteData->connection,sqlite3_close);
        IF_DO_NULL(sqliteData->tableName,CS_stringFree);
        IF_DO_NULL(sqliteData->dbFile,CS_stringFree);
        IF_DO_NULL(sqliteData->sb,CS_SB_free);
        CS_free( sqliteData );
    }
}
#define OR_NULL(_X) (_X)?(_X):"NULL"
static int sqlCallback( void *storagePointer, int numberOfColumns, char **columnDatas, char **columnNames ) {
    struct CS_StringBuilder *sb = ((struct privateSqliteData*)storagePointer)->sb;
    CS_SB_reset(sb);
    CS_SB_printf( sb, "SQL: %d columns: {", numberOfColumns );
    for( int i = 0; i < numberOfColumns; ++i ) {
        CS_SB_printf( sb, "{\"%s\":\"%s\"}", OR_NULL(columnNames[i]), OR_NULL(columnDatas[i]) );
        if( i < numberOfColumns - 1 ) CS_SB_appendChar(sb, ',');
    }
    CS_SB_appendChar(sb,'}');
    CS_LOG_LOUD("%s", sb->buffer);
    return 0;
}

static struct privateSqliteData *privateCreateSqliteFromConfig( const char *config ) {
    struct privateSqliteData *returnValue = (struct privateSqliteData *)CS_allocZero( sizeof( struct privateSqliteData ) );
    if( returnValue ) {
        returnValue->sb = CS_SB_create( 4096 );
        returnValue->tableName = NULL;
        returnValue->dbFile = NULL;
        if( config ) {
            char *commasStorage;
            char *equalsStorage;
            char *temp = CS_tempStringCopy( config );
            char *nextItem = strtok_r( temp, ",", &commasStorage );
            do {
                char *key = strtok_r( nextItem, "=", &equalsStorage );
                char *val = strtok_r( NULL, "=", &equalsStorage );
                if( key && val ) {
                    if( strcmp( key, "file" ) == 0 ) {
                        if( returnValue->dbFile ) CS_stringFree( returnValue->dbFile );
                        returnValue->dbFile = CS_stringCopy( val );
                    } else if ( strcmp( key, "table" ) == 0 ) {
                        if( returnValue->tableName ) CS_stringFree( returnValue->tableName );
                        returnValue->tableName = CS_stringCopy( val );
                    }
                }
            } while( (nextItem = strtok_r( NULL, ",", &commasStorage )) != NULL );
        }
        if( returnValue->tableName == NULL ) returnValue->tableName = CS_stringCopy( sqliteDefaultTableName );
        if( returnValue->dbFile == NULL ) returnValue->dbFile = CS_stringCopy( sqliteDefaultDbFile );

        if( sqlite3_open_v2( returnValue->dbFile, &returnValue->connection, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, NULL ) != SQLITE_OK ) {
            CS_LOG_ERROR("Failed to open sqlite db %s", returnValue->dbFile);
            privateFreeSqlite(returnValue);
            return NULL;
        }

        struct CS_StringBuilder *sb = CS_SB_create( 4096 );
        CS_SB_printf( sb, "CREATE TABLE IF NOT EXISTS %s (\n", returnValue->tableName );
        CS_SB_printf( sb, "    key TEXT(128) PRIMARY KEY,\n" );
        CS_SB_printf( sb, "    cas INT(11),\n" );
        CS_SB_printf( sb, "    value BLOB ) WITHOUT ROWID" );

        char *errorMessage;
        sqlite3_exec( returnValue->connection, sb->buffer, sqlCallback, (void*)returnValue, &errorMessage );
        if( errorMessage ) {
            CS_LOG_ERROR("SQL: error message %s", errorMessage );
            sqlite3_free( errorMessage );
        }

        CS_SB_reset( sb );
        CS_SB_printf(sb, "SELECT cas, value FROM %s WHERE key = ?", returnValue->tableName);
        if( sqlite3_prepare_v2( returnValue->connection, sb->buffer, -1, &returnValue->get, NULL ) != SQLITE_OK ) CS_LOG_ERROR("SQL: %s: %s", sb->buffer, sqlite3_errmsg( returnValue->connection ) );

        CS_SB_reset( sb );
        CS_SB_printf(sb, "UPDATE %s SET cas = ?, value = ? WHERE key = ? AND cas = ?", returnValue->tableName);
        if( sqlite3_prepare_v2( returnValue->connection, sb->buffer, -1, &returnValue->update, NULL ) != SQLITE_OK ) CS_LOG_ERROR("SQL: %s: %s", sb->buffer, sqlite3_errmsg( returnValue->connection ) );

        CS_SB_reset( sb );
        CS_SB_printf(sb, "INSERT INTO %s ( key, cas, value ) VALUES (?, ?, ?)", returnValue->tableName );
        if( sqlite3_prepare_v2( returnValue->connection, sb->buffer, -1, &returnValue->put, NULL ) != SQLITE_OK ) CS_LOG_ERROR("SQL: %s: %s", sb->buffer, sqlite3_errmsg( returnValue->connection ) );

        CS_SB_reset( sb );
        CS_SB_printf(sb, "DELETE FROM %s WHERE key = ?", returnValue->tableName );
        if( sqlite3_prepare_v2( returnValue->connection, sb->buffer, -1, &returnValue->remove, NULL ) != SQLITE_OK ) CS_LOG_ERROR("SQL: %s: %s", sb->buffer, sqlite3_errmsg( returnValue->connection ) );

        CS_SB_free( sb );
    }
    return returnValue;
}

static struct CS_Storage *privateSqliteOpen(struct CS_Storage *storage) {
    storage->storageData = (const void *)privateCreateSqliteFromConfig( storage->config );
    return storage->storageData?storage:NULL;
}
static bool privateSqliteClose(struct CS_Storage *storage) {
    struct privateSqliteData *sqliteData = (struct privateSqliteData*)storage->storageData;
    if( sqliteData ) privateFreeSqlite( sqliteData );
    storage->storageData = NULL;
    return false;
}
static struct CS_StorageItem *privateSqliteGet(const struct CS_Storage *storage, const char *key) {
    struct privateSqliteData *sqliteData = (struct privateSqliteData*)storage->storageData;
    sqlite3_bind_text( sqliteData->get, 1, key, -1, SQLITE_STATIC );
    struct CS_StorageItem *returnValue = NULL;
    if( sqlite3_step( sqliteData->get ) == SQLITE_ROW ) {
        unsigned int cas = sqlite3_column_int( sqliteData->get, 0 );
        const void *value = sqlite3_column_blob( sqliteData->get, 1 );
        unsigned int size = sqlite3_column_bytes( sqliteData->get, 1 );
        returnValue = privateGetStorageItemWithCopyData( key, cas, size, value );
    }
    sqlite3_reset( sqliteData->get );
    return returnValue;
}
static struct CS_StorageItem *privateSqlitePut(const struct CS_Storage *storage, struct CS_StorageItem *item ) {
    struct CS_StorageItem *returnValue = item;
    struct privateSqliteData *sqliteData = (struct privateSqliteData*)storage->storageData;
    item->cas++;
    sqlite3_stmt *toStep;
    if( item->cas == 1 ) {
        //Key, Cas, Value
        sqlite3_bind_text( sqliteData->put, 1, item->key, -1, SQLITE_STATIC );
        sqlite3_bind_int( sqliteData->put, 2, item->cas );
        sqlite3_bind_blob( sqliteData->put, 3, item->value, item->size, SQLITE_STATIC );
        toStep = sqliteData->put;
    } else {
        //Cas, Value, Key, OldCas
        sqlite3_bind_int( sqliteData->update, 1, item->cas );
        sqlite3_bind_blob( sqliteData->update, 2, item->value, item->size, SQLITE_STATIC );
        sqlite3_bind_text( sqliteData->update, 3, item->key, -1, SQLITE_STATIC );
        sqlite3_bind_int( sqliteData->update, 4, item->cas - 1 );
        toStep = sqliteData->update;
    }
    if( sqlite3_step( toStep ) != SQLITE_DONE )
        returnValue = NULL;
    sqlite3_reset( toStep );
    //If the add or update failed, return the item in question.
    if( returnValue == NULL ) return privateSqliteGet( storage, item->key );
    return item;
}
static bool privateSqliteRemove(const struct CS_Storage *storage, const char *key) {
    bool returnValue = false;
    struct privateSqliteData *sqliteData = (struct privateSqliteData*)storage->storageData;
    sqlite3_bind_text( sqliteData->remove, 1, key, -1, SQLITE_STATIC );
    if( sqlite3_step( sqliteData->remove ) != SQLITE_DONE ) returnValue = true;
    sqlite3_reset( sqliteData->remove );
    return returnValue || sqlite3_changes(sqliteData->connection) != 1;
}
/******************************************************************************************/
static struct CS_Storage *privateHashtableOpen(struct CS_Storage *storage) {
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
static bool privateHashtableClose(struct CS_Storage *storage) {
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
static struct CS_StorageItem *privateHashtableGet(const struct CS_Storage *storage, const char *key) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    const void *maybe = CS_hashtableGet( table, key );
    if( maybe == CS_HASHTABLE_ERROR ) return NULL;
    return privateDeepCopyStorageItem( (const struct CS_StorageItem *)maybe );
}
struct _checkContext {
    const struct CS_Storage *storage;
    struct CS_StorageItem *oldItemCopy;
    int casToCheck;
};
//If we didn't insert...
static int privateHashtableCheckPutMaybe( void *context, const void *previousItemInTable ) {
    struct _checkContext *checkContext = (struct _checkContext *)context;
    const struct CS_StorageItem *oldItem = (const struct CS_StorageItem *)previousItemInTable;
    if( checkContext->casToCheck == 0 ) {
        if( previousItemInTable && previousItemInTable != CS_HASHTABLE_ERROR ) {
            //Trying to insert, but there was something there already. Return a copy of
            //what's in the table. 
            checkContext->oldItemCopy = privateDeepCopyStorageItem( oldItem );
            return CS_HASHTABLE_MAYBE_RETURN_ERROR;
        }
        //We can insert our current thing.
        return CS_HASHTABLE_MAYBE_PUT_RETURN_NULL;
    }
    //Cas wasn't 0...but there no previous item to check. Return an error.
    if( previousItemInTable == NULL ) {
        return CS_HASHTABLE_MAYBE_RETURN_ERROR;
    }
    //Cas doesn't match. Don't swap it but return a copy of the old.
    if(checkContext->casToCheck != oldItem->cas) {
        checkContext->oldItemCopy = privateDeepCopyStorageItem( oldItem );
        return CS_HASHTABLE_MAYBE_RETURN_ERROR;
    }
    return CS_HASHTABLE_MAYBE_PUT_RETURN_OLD;
}
static struct CS_StorageItem *privateHashtablePut(const struct CS_Storage *storage, struct CS_StorageItem *item ) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    struct _checkContext context = { storage, NULL, item->cas };
    item->cas++;
    struct CS_StorageItem *itemCopy = privateDeepCopyStorageItem(item);
    if( itemCopy == NULL ) return NULL;
    struct CS_StorageItem *old = (struct CS_StorageItem *)CS_hashtablePutMaybe(table, item->key, itemCopy, &context,privateHashtableCheckPutMaybe);
    if( old && old != CS_HASHTABLE_ERROR ) privateReturnStorageItem(old);
    if( old == CS_HASHTABLE_ERROR ) {
        //We failed to put the new item in, return a copy of the old one (or possibly NULL)
        privateReturnStorageItem(itemCopy);
        return context.oldItemCopy;
    }
    return item;
}
static bool privateHashtableRemove(const struct CS_Storage *storage, const char *key) {
    struct CS_HashTable *table = (struct CS_HashTable *)storage->storageData;
    struct CS_StorageItem *oldItem = (struct CS_StorageItem *)CS_hashtableRemove( table, key );
    if( oldItem && oldItem != CS_HASHTABLE_ERROR ) privateReturnStorageItem( oldItem );
    return oldItem==CS_HASHTABLE_ERROR?true:false;
}
