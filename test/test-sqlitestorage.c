#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/storage.h>

extern bool test_sqlitestorage(void);

static int testCount = 0;
static int testSucceeded = 0;

extern bool util_test_generic_storage( const struct CS_Storage *storage );

bool test_sqlitestorage(void) {
    //Tests go here:
    unlink("a.sqlite");
    const struct CS_Storage *storage = CS_storageOpen("Test", "file=a.sqlite", CS_STORAGE_BACKEND_SQLITE );

    CS_FAIL_ON_NULL( storage, "Create sqlite storage with no config.", "Failed");

    if( storage ) {
        CS_FAIL_ON_TRUE( util_test_generic_storage( storage ), "Testing storage in a.sqlite", "Failed." );
        
        CS_storageClose( storage );
    }
    CS_storageTeardown();
    return testCount !=
           testSucceeded;
}


