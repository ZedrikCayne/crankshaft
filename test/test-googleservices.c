#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/storage.h>

#include <crankshaft/googleservices.h>
#include <crankshaft/jwtkeychain.h>

extern bool test_googleservices(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_googleservices(void) {
    const struct CS_Storage *keysCacheBackingStorage = CS_storageOpen( "KEY_WEB_CACHE", "file=/tmp/crankshaft_key.sqlite", CS_STORAGE_BACKEND_SQLITE );
    CS_jwtkeychainInit( keysCacheBackingStorage );
    //Tests go here:
    CS_FAIL_ON_TRUE( CS_GS_getKeys(), "Get public google keys.", "Failed to grab them." );
    struct CS_StringBuilder *sb = CS_GS_getKeysDesc();

    CS_FAIL_ON_NULL( sb, "Failed to get key desc.", "Oops." );
    if( sb ) {
        CS_LOG_INFO("%s", CS_SB_buffer( sb ) );
        CS_SB_free(sb);
    }

    return testCount !=
           testSucceeded;
}


