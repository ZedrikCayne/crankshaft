#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/hashtable.h>
#include <crankshaft/uuid.h>
#include <crankshaft/slaballoc.h>
#include <stdint.h>

extern bool test_hashtable(void);

static int32_t testCount = 0;
static int32_t testSucceeded = 0;

#define THINGS_TO_ADD 128

bool test_hashtable(void) {
    //Tests go here:
    CS_uuidInit();

    const char ** keys = CS_alloc( THINGS_TO_ADD * sizeof( char * ) );
    if( !keys ) return true;

    for( int32_t i = 0; i < THINGS_TO_ADD; ++i ) {
        keys[ i ] = CS_uuid4Cstring();
    }

    struct CS_HashTable *stringVoid = CS_HASHTABLE_STRING_VOID( 32, CS_HASHTABLE_FLAG_VERY_PEDANTIC );
    CS_FAIL_ON_NULL(stringVoid, "Creating a default hash table with 32 entries.", "Failed." );
    if( stringVoid ) {
        for( int32_t i = 0; i < THINGS_TO_ADD; ++i ) if( keys[ i ] ) {
            CS_FAIL_ON_NOT_NULL( CS_hashtablePut( stringVoid, keys[ i ], keys[ i ] ), "Adding means we've got a duplicate key.", "Duplicate key?" );
        }
        int32_t count = 0;
        CS_HASHTABLE_ITER( stringVoid, entry ) {
            CS_FAIL_ON_FALSE( strcmp(entry->fullKey, entry->value) == 0, "Key/value pair match?", "Nope");
            ++count;
        }
        CS_FAIL_ON_FALSE( count == THINGS_TO_ADD, "Iterate count should match number put in.", "Buh %d vs %d", count, THINGS_TO_ADD );
        CS_hashtableResize( stringVoid, 48 );
        CS_HASHTABLE_ITER( stringVoid, entry ) {
            CS_FAIL_ON_FALSE( strcmp(entry->fullKey, entry->value) == 0, "Key/value pair match?", "Nope");
            ++count;
        }
        CS_hashtableFree( stringVoid );
    }

    void *keyAllocatorForHashTable = CS_slabInit("UUID keys",sizeof(struct CS_UUID),32,8);
    struct CS_HashTable *uuidVoid = CS_HASHTABLE_UUID_VOID( 32, CS_HASHTABLE_FLAG_VERY_PEDANTIC, keyAllocatorForHashTable );
    CS_FAIL_ON_NULL(uuidVoid, "Creating a default UUID hash table with 32 entries.", "Failed" );
    if( uuidVoid ) {
        for( int32_t i = 0; i < THINGS_TO_ADD; ++i ) {
            CS_FAIL_ON_NOT_NULL( CS_hashtablePut( uuidVoid, CS_uuidFromCstringTemp( (char*)keys[ i ], UUID_CHAR_SIZE_BYTES ), keys[ i ] ), "Getting a non null here means we have a duplicated key.", "Duplicate key?" );
        }
        int32_t count = 0;
        CS_HASHTABLE_ITER( uuidVoid, entry ) {
            CS_FAIL_ON_FALSE( strcmp( CS_uuidToCstringTemp(entry->fullKey), entry->value ) == 0,
                    "UUID key turned into a string should match the input UUID string.", "Nope." );
            ++count;
        }
        CS_FAIL_ON_FALSE( count == THINGS_TO_ADD, "Iterate count should match number put in.", "Buh %d vs %d", count, THINGS_TO_ADD );
        CS_hashtableResize( uuidVoid, 48 );
        CS_HASHTABLE_ITER( uuidVoid, entry ) {
            CS_FAIL_ON_FALSE( strcmp( CS_uuidToCstringTemp(entry->fullKey), entry->value ) == 0,
                    "UUID key turned into a string should match the input UUID string.", "Nope." );
            ++count;
        }
        CS_hashtableFree( uuidVoid );
    }

    for( int32_t i = 0; i < THINGS_TO_ADD; ++i ) {
        CS_uuidFreeCstring( keys[ i ] );
    }
    CS_free( keys );

    CS_uuidKill();

    return testCount !=
           testSucceeded;
}


