#ifndef __crankshaftcachedoth__
#define __crankshaftcachedoth__
#include <stdbool.h>

#include <crankshaft/stringbuilder.h>
#include <crankshaft/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Cache;

struct CS_StorageItem *CS_cacheGet(const struct CS_Cache *cache, const char *key);
struct CS_StorageItem *CS_cachePut(const struct CS_Cache *cache, const char *key, void *value, size_t size, time_t expires);
bool CS_cacheClear(const struct CS_Cache *cache, const char *key);
bool CS_cacheClearAll(const struct CS_Cache *cache);
void *CS_cacheGetCacheData(const struct CS_Cache *cache);
struct CS_StringBuilder *CS_describeCache(const struct CS_Cache *cache);

const struct CS_Cache *CS_cacheCreate(
        const struct CS_Storage *backingStorage,
        struct CS_StorageItem *(*fetch)(const struct CS_Cache *cache, const char *key),
        bool (*deleteCacheData)(const struct CS_Cache* cache),
        void *cacheData);
bool CS_cacheDestroy(const struct CS_Cache *cache);

#ifdef __cplusplus
}
#endif
#endif
