#include <stdlib.h>
#include <stdio.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/cache.h>

struct CS_Cache {
    const struct CS_Storage *backingStorage;
    struct CS_StorageItem *(*fetch)(const struct CS_Cache *cache, const char *key);
    bool (*deleteCacheData)(const struct CS_Cache *cache);
    void *cacheData;
};

struct CS_StorageItem *CS_cacheGet(const struct CS_Cache *cache, const char *key) {
    struct CS_StorageItem *returnValue = CS_storageGet( cache->backingStorage, key );
    if( !returnValue ) {
        return cache->fetch( cache, key );
    }
    return returnValue;
}

struct CS_StorageItem *CS_cachePut(const struct CS_Cache *cache, const char *key, void *value, size_t size, time_t expires) {
    struct CS_StorageItem *returnValue = CS_storageGet( cache->backingStorage, key );
    if( !returnValue ) {
        return CS_storagePut( cache->backingStorage, key, value, size, expires, NULL );
    }
    return returnValue;
}

bool CS_cacheClear(const struct CS_Cache *cache, const char *key) {
    return CS_storageRemove( cache->backingStorage, key );
}

bool CS_cacheClearAll(const struct CS_Cache *cache) {
    return false;
}

const struct CS_Cache *CS_cacheCreate(
        const struct CS_Storage *backingStorage,
        struct CS_StorageItem *(*fetch)(const struct CS_Cache *cache, const char *key), 
        bool (*deleteCacheData)(const struct CS_Cache* cache),
        void *cacheData) {
    if( backingStorage == NULL ) {
        CS_LOG_ERROR("backingStorage is null.");
        return NULL;
    }
    struct CS_Cache *returnValue = CS_allocZero(sizeof(struct CS_Cache));
    if( returnValue == NULL ) {
        CS_LOG_ERROR("OOM Creating a cache.");
        return NULL;
    }
    returnValue->backingStorage = backingStorage;
    returnValue->fetch = fetch;
    returnValue->deleteCacheData = deleteCacheData;
    returnValue->cacheData = cacheData;
    return returnValue;
}

bool CS_cacheDestroy(const struct CS_Cache *cache) {
    struct CS_Cache *writableCache = (struct CS_Cache *)cache;
    if( cache == NULL )
        return true;
    bool returnValue = false;
    if( cache->deleteCacheData && cache->cacheData ) {
        returnValue |= cache->deleteCacheData(cache);
        writableCache->cacheData = NULL;
    }
    returnValue |= CS_storageClose( cache->backingStorage );
    writableCache->backingStorage = NULL;
    CS_free((void*)cache);
    return returnValue;
}

void *CS_cacheGetCacheData(const struct CS_Cache *cache) {
    return cache->cacheData;
}


struct CS_StringBuilder *CS_describeCache(const struct CS_Cache *cache) {
    struct CS_StringBuilder *returnValue = CS_SB_create( 8192 );
    if( !cache ) {
        CS_SB_append(returnValue, "{null}");
    } else {
    }

    return returnValue;
}
