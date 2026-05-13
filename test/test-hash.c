#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/uuid.h>
#include <crankshaft/hash.h>
#include <stdint.h>

extern bool test_hash(void);

static int32_t testCount = 0;
static int32_t testSucceeded = 0;

#define UUIDS_TO_MAKE 200

bool test_hash(void) {
    //Tests go here:
    CS_uuidSetSeed( CS_testRand() );
    CS_uuidInit();

    int32_t *uuidHashes = CS_alloc( UUIDS_TO_MAKE * sizeof(int32_t) );
    const char **uuidStrings = CS_alloc( UUIDS_TO_MAKE * sizeof(char *) );
    for( int32_t i = 0; i < UUIDS_TO_MAKE; ++i ) {
        const char *textUuid = CS_uuid4String();
        uuidStrings[ i ] = textUuid;
        uuidHashes[ i ] = CS_hash(textUuid);
        for( int32_t j = i - 1; j >= 0; j-- ) CS_FAIL_ON_TRUE( uuidHashes[ i ] == uuidHashes[ j ], "Hashes should not match.", "Buh? uuidStrings[%d]'%s' vs. uuidStrings[%d]'%s'", i, uuidStrings[i], j, uuidStrings[j] );
    }
    for( int32_t i = 0; i < UUIDS_TO_MAKE; ++i ) {
        CS_uuidFreeString( uuidStrings[i] );
    }
    CS_free( uuidHashes );
    CS_free( uuidStrings );


    CS_uuidKill();


    return testCount !=
           testSucceeded;
}


