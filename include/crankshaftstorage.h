#ifndef __crankshaftstoragedoth__
#define __crankshaftstoragedoth__

#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_StorageItem {
    unsigned int cas;
    unsigned int size;
    const char *key;
    void *value;
};

struct CS_Storage;

//open -> initializes stuff...probably sets the storageData with whatever implemention specific data is required
//close -> closes the connection/etc.
//get -> returns a 'new' copy of a CS_StorageItem with the data. NULL if missing.
//getCas -> returns the CAS of the current thing. 0 if it doesn't exist
//put -> Either returns a 'new' copy of whatever was out there already, NULL if it was
//    -> missing and we wanted to update it, or the passed in item. (If the input cas
//    -> was 0, it's 'new' otherwise it's an update. if the cas doesn't match it'll return
//    -> a new storage item that we can update.
//remove -> removes old item, returns 'new' copy of the removed data, NULL on error.
struct CS_StorageBackend {
    const char *name;
    struct CS_Storage *(*open)(struct CS_Storage *storage);
    bool (*close)(struct CS_Storage *storage);
    struct CS_StorageItem *(*get)(const struct CS_Storage *storage, const char *key);
    unsigned int (*getCas)(const struct CS_Storage *storage, const char *key);
    struct CS_StorageItem *(*put)(const struct CS_Storage *storage, struct CS_StorageItem *item);
    struct CS_StorageItem *(*remove)(const struct CS_Storage *storage, const char *key);
};

struct CS_Storage {
    const char *name;
    const char *config;
    const struct CS_StorageBackend *backend;
    const void *storageData;
    pthread_mutex_t storageMutex;
};

const struct CS_Storage *CS_storageGetStorage( const char *storageName );
const struct CS_Storage *CS_storageOpen(const char *storageName, const char *config, const struct CS_StorageBackend *backend);
bool CS_storageClose(const struct CS_Storage *closeMe);
void CS_storageTeardown(void);
struct CS_StorageItem *CS_storageGet(const struct CS_Storage *storage, const char *key );
struct CS_StorageItem *CS_storageRemove( const struct CS_Storage *storage, const char *key );
struct CS_StorageItem *CS_storageUpdate( const struct CS_Storage *storage, struct CS_StorageItem *updated );
struct CS_StorageItem *CS_storagePut( const struct CS_Storage *storage, const char *key, const void *value, int size );
void CS_storageReturnItem( struct CS_StorageItem *item );

extern const struct CS_StorageBackend *CS_STORAGE_BACKEND_SQLITE;
extern const struct CS_StorageBackend *CS_STORAGE_BACKEND_HASHTABLE;

#ifdef __cplusplus
}
#endif
#endif
