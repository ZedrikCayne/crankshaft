#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

#include "crankshaftuuid.h"
#include "crankshafthash.h"

extern bool test_hash(void);

static int testCount = 0;
static int testSucceeded = 0;

#define UUIDS_TO_MAKE 200

bool test_hash(void) {
    //Tests go here:
    CS_uuidSetSeed( CS_testRand() );
    CS_uuidInit();

    int *uuidHashes = CS_alloc( UUIDS_TO_MAKE * sizeof(int) );
    const char **uuidStrings = CS_alloc( UUIDS_TO_MAKE * sizeof(char *) );
    for( int i = 0; i < UUIDS_TO_MAKE; ++i ) {
        const char *textUuid = CS_uuid4String();
        uuidStrings[ i ] = textUuid;
        uuidHashes[ i ] = CS_hash(textUuid);
        for( int j = i - 1; j >= 0; j-- ) CS_FAIL_ON_TRUE( uuidHashes[ i ] == uuidHashes[ j ], "Hashes should not match.", "Buh? uuidStrings[%d]'%s' vs. uuidStrings[%d]'%s'", i, uuidStrings[i], j, uuidStrings[j] );
    }
    for( int i = 0; i < UUIDS_TO_MAKE; ++i ) {
        CS_uuidFreeString( uuidStrings[i] );
    }
    CS_free( uuidHashes );
    CS_free( uuidStrings );


    CS_uuidKill();


    return testCount !=
           testSucceeded;
}


