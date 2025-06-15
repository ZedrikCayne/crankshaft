#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/cache.h>

extern bool test_cache(void);

static int testCount = 0;
static int testSucceeded = 0;

static int numFetches = 0;
static bool fetchFail = false;
static bool lowercaseV = false;

static struct CS_StorageItem *testFetch( const struct CS_Cache *cache, const char *key ) {
    if( fetchFail ) {
        fetchFail = false;
        return NULL;
    }
    ++numFetches;
    char *temp = CS_tempBuffSnprintf( 32, "%s", key );
    if( lowercaseV ) {
        temp[0] = 'v';
    } else {
        temp[0] = 'V';
    }
    temp[1] = 'a';
    temp[2] = 'l';
    return CS_cachePut( cache, key, temp, strlen(key) + 1, time(NULL) + 2 );
}

#define OR_NULL(X) (X?(char *)X:"NULL")

static bool testStorageDataDelete( const struct CS_Cache *cache ) {
    void *cacheData = CS_cacheGetCacheData( cache );
    if( cacheData ) {
        CS_free( cacheData );
    }
    return false;
}


#define NUM_ITEMS 35
#define SKIPS 3

bool test_cache(void) {
    //Tests go here:
    const struct CS_Storage *backingStorage = CS_storageOpen("cache_test_storage", NULL, CS_STORAGE_BACKEND_HASHTABLE);
    CS_FAIL_ON_NULL(backingStorage, "Create temp storage.", "Failed to create temp storage.");
    if( backingStorage ) {
        const struct CS_Cache *testCache = CS_cacheCreate( backingStorage, testFetch, testStorageDataDelete, CS_alloc(1024) );
        CS_FAIL_ON_NULL( testCache, "Create test cache.", "Failed to create test cache.");

        if( testCache ) {
            for( int i = 0; i < NUM_ITEMS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Key %d", i );
                char *expectedVal = CS_tempBuffSnprintf( 64, "Val %d", i );
                struct CS_StorageItem *item = CS_cacheGet( testCache, key );
                CS_FAIL_ON_NULL( item, CS_tempBuffSnprintf( 64, "Trying to stuff in an item %d.", i ), "Failed." );
                if( item ) {
                    CS_FAIL_ON_FALSE( strncmp( expectedVal, item->value, 64 ) == 0, CS_tempBuffSnprintf( 64, "Match expected value %d", i ), "%s vs %s", OR_NULL(expectedVal), OR_NULL(item->value) );
                    CS_storageReturnItem( item );
                }
            }
            //Set the 'fetch' so it'll put in lowercase val.
            for( int i = 0; i < NUM_ITEMS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Key %d", i );
                char *expectedVal = CS_tempBuffSnprintf( 64, "Val %d", i );
                lowercaseV = true;
                struct CS_StorageItem *item = CS_cacheGet( testCache, key );
                CS_FAIL_ON_NULL( item, CS_tempBuffSnprintf( 64, "Trying to lowercase stuff in an item %d.", i ), "Failed." );
                if( item ) {
                    CS_FAIL_ON_FALSE( strncmp( expectedVal, item->value, 64 ) == 0, CS_tempBuffSnprintf( 64, "Match expected value %d", i ), "%s vs %s", OR_NULL(expectedVal), OR_NULL(item->value) );
                    CS_storageReturnItem( item );
                }
            }

            sleep(3);

            for( int i = 0; i < NUM_ITEMS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Key %d", i );
                char *expectedVal = CS_tempBuffSnprintf( 64, "val %d", i );
                lowercaseV = true;
                struct CS_StorageItem *item = CS_cacheGet( testCache, key );
                CS_FAIL_ON_NULL( item, CS_tempBuffSnprintf( 64, "Trying to stuff in an item after expire %d.", i ), "Failed." );
                if( item ) {
                    CS_FAIL_ON_FALSE( strncmp( expectedVal, item->value, 64 ) == 0, CS_tempBuffSnprintf( 64, "Match expected value %d", i ), "%s vs %s", OR_NULL(expectedVal), OR_NULL(item->value) );
                    CS_storageReturnItem( item );
                }
            }
            CS_cacheDestroy( testCache );
        }
    }
    CS_storageTeardown();

    return testCount !=
           testSucceeded;
}


