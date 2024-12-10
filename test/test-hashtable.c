#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshafthashtable.h"
#include "crankshaftuuid.h"
#include "crankshaftslaballoc.h"

extern bool test_hashtable(void);

static int testCount = 0;
static int testSucceeded = 0;

#define THINGS_TO_ADD 128

bool test_hashtable(void) {
    //Tests go here:
    CS_uuidInit();

    const char ** keys = CS_alloc( THINGS_TO_ADD * sizeof( char * ) );
    if( !keys ) return true;

    for( int i = 0; i < THINGS_TO_ADD; ++i ) {
        keys[ i ] = CS_uuid4String();
    }

    struct CS_HashTable *stringVoid = CS_HASHTABLE_STRING_VOID( 32, CS_HASHTABLE_FLAG_VERY_PEDANTIC );
    CS_FAIL_ON_NULL(stringVoid, "Creating a default hash table with 32 entries.", "Failed." );
    if( stringVoid ) {
        for( int i = 0; i < THINGS_TO_ADD; ++i ) if( keys[ i ] ) {
            CS_FAIL_ON_NOT_NULL( CS_hashtablePut( stringVoid, keys[ i ], keys[ i ] ), "Adding means we've got a duplicate key.", "Duplicate key?" );
        }
        int count = 0;
        CS_HASHTABLE_ITER( stringVoid, entry ) {
            CS_FAIL_ON_FALSE( strcmp(entry->fullKey, entry->value) == 0, "Key/value pair match?", "Nope");
            ++count;
        }
        CS_FAIL_ON_FALSE( count == THINGS_TO_ADD, "Iterate count should match number put in.", "Buh %d vs %d", count, THINGS_TO_ADD );
        CS_hashtableFree( stringVoid );
    }

    void *keyAllocatorForHashTable = CS_initSlabAlloc("UUID keys",sizeof(struct CS_UUID),32,4);
    struct CS_HashTable *uuidVoid = CS_HASHTABLE_UUID_VOID( 32, CS_HASHTABLE_FLAG_VERY_PEDANTIC, keyAllocatorForHashTable );
    CS_FAIL_ON_NULL(uuidVoid, "Creating a default UUID hash table with 32 entries.", "Failed" );
    if( uuidVoid ) {
        for( int i = 0; i < THINGS_TO_ADD; ++i ) {
            CS_FAIL_ON_NOT_NULL( CS_hashtablePut( uuidVoid, CS_uuidFromStringTemp( (char*)keys[ i ], UUID_CHAR_SIZE_BYTES ), keys[ i ] ), "Getting a non null here means we have a duplicated key.", "Duplicate key?" );
        }
        int count = 0;
        CS_HASHTABLE_ITER( uuidVoid, entry ) {
            CS_FAIL_ON_FALSE( strcmp( CS_uuidToStringTemp(entry->fullKey), entry->value ) == 0,
                    "UUID key turned into a string should match the input UUID string.", "Nope." );
            ++count;
        }
        CS_FAIL_ON_FALSE( count == THINGS_TO_ADD, "Iterate count should match number put in.", "Buh %d vs %d", count, THINGS_TO_ADD );
        CS_hashtableFree( uuidVoid );
    }

    for( int i = 0; i < THINGS_TO_ADD; ++i ) {
        CS_free( (void*)keys[ i ] );
    }
    CS_free( keys );

    CS_uuidKill();

    return testCount !=
           testSucceeded;
}


