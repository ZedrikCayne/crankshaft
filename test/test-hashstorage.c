#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

#include "crankshaftstorage.h"

extern bool test_hashstorage(void);
extern bool util_test_generic_storage(const struct CS_Storage * storage);

static int testCount = 0;
static int testSucceeded = 0;

bool test_hashstorage(void) {
    //Tests go here:
    const struct CS_Storage *storage = CS_storageOpen("Test", NULL, CS_STORAGE_BACKEND_HASHTABLE);
    CS_FAIL_ON_TRUE( util_test_generic_storage( storage ), "Testing hashtable Storage.", "Failed" );
    CS_storageClose( storage );
    CS_storageTeardown();
    return testCount !=
           testSucceeded;
}


