#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/uuid.h>

extern bool test_uuid(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_uuid(void) {
    //Tests go here:
    CS_uuidInit();
    int uuidRandTest = CS_testRand();
    CS_uuidSetSeed(uuidRandTest);

    const struct CS_UUID *uuid = CS_uuid4();

    CS_uuidSetSeed(uuidRandTest);

    const struct CS_UUID *uuid2 = CS_uuid4();

    for( int i = 0; i < UUID_BYTES_IN_UUID; ++i ) {
        CS_FAIL_ON_FALSE( uuid->uuid[i] == uuid2->uuid[ i ], "Checking uuids match on same seed.", "No match!");
    }

    const char *tempOne = CS_uuidToStringTemp( uuid );
    const char *tempTwo = CS_uuidToStringTemp( uuid2 );
    
    CS_uuidFree(uuid);
    CS_uuidFree(uuid2);
    uuid = NULL;
    uuid2 = NULL;

    CS_FAIL_ON_FALSE( strncmp( tempOne, tempTwo, UUID_CHAR_SIZE_BYTES ) == 0, "Two uuids that are the same should come to the same string.", "Not %s:%s", tempOne?tempOne:"NULL", tempTwo?tempTwo:"NULL" );

    CS_uuidKill();
    return testCount !=
           testSucceeded;
}


