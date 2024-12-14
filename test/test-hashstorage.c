#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

#include "crankshaftstorage.h"

extern bool test_hashstorage(void);

static int testCount = 0;
static int testSucceeded = 0;

#define NUM_THINGS 255
#define MAX_SIZE_OF_THING 1024

struct Thing {
    char *buffer;
    int size;
};

static struct Thing *newThing() {
    struct Thing *newThing = CS_alloc( sizeof(struct Thing) );
    if( newThing ) {
        newThing->size = CS_testRandMax( MAX_SIZE_OF_THING - 1 ) + 1;
        newThing->buffer = CS_alloc( newThing->size );
        if( newThing->buffer ) {
            for( int i = 0; i < newThing->size; ++i ) {
                newThing->buffer[ i ] = CS_testRandMax( 255 );
            }
        } else {
            CS_free( newThing );
            newThing = NULL;
        }
    }

    return newThing;
}

static struct Thing *copyThing( struct Thing *copyMe ) {
    struct Thing *newThing = CS_alloc( sizeof(struct Thing) );
    if( newThing ) {
        newThing->size = copyMe->size;
        newThing->buffer = CS_alloc( newThing->size );
        if( newThing->buffer ) {
            for( int i = 0; i < newThing->size; ++i ) {
                newThing->buffer[ i ] = copyMe->buffer[ i ];
            }
        } else {
            CS_free( newThing );
            newThing = NULL;
        }
    }
    return newThing;
}

static void freeThing( struct Thing *aThing ) {
    if( aThing ) {
        if( aThing->buffer ) { CS_free( aThing->buffer ); aThing->buffer = NULL; };
        aThing->size = 0;
        CS_free( aThing );
    }
}

static void modifyThing( struct Thing *aThing ) {
    if( aThing ) {
        if( aThing->buffer ) {
            int index = CS_testRandMax( aThing->size - 1 );
            aThing->buffer[index] = aThing->buffer[index] ^ 0x34;
        }
    }
}

static bool compareThing( struct Thing *aThing, struct Thing *bThing ) {
    if( aThing->size != bThing->size ) return true;
    for( int i = 0; i < aThing->size; ++i ) {
        if( aThing->buffer[ i ] != bThing->buffer[ i ] ) {
            return true;
        }
    }
    return false;
}

static bool compareThingToStorageItem( struct Thing *aThing, struct CS_StorageItem *item ) {
    struct Thing tempThing = { (char*)item->value, item->size };
    return compareThing( aThing, &tempThing );
}

bool test_hashstorage(void) {
    //Tests go here:
    const struct CS_Storage *storage = CS_storageOpen("Test", NULL, CS_STORAGE_BACKEND_HASHTABLE);

    CS_FAIL_ON_NULL( storage, "Create hashtable storage with no config.", "Failed");
    
    struct Thing ** stored = CS_alloc( sizeof(struct Thing) * NUM_THINGS );
    struct Thing ** stored2 = CS_alloc( sizeof(struct Thing) * NUM_THINGS );
    if(stored) {
        for( int i = 0; i < NUM_THINGS; ++i ) {
            stored[ i ] = newThing();
            stored2[ i ] = copyThing( stored[ i ] );
        }
        bool bError = false;
        for( int i = 0; i < NUM_THINGS; ++i ) {
            if( !stored[ i ] ) { bError = true; break; };
            if( !stored2[ i ] ) { bError = true; break; };
        }
        if( bError ) {
            for( int i = 0; i < NUM_THINGS; ++i ) {
                freeThing( stored[ i ] );
                stored[ i ] = NULL;
            }
        } else {
            //Store everything.
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *putIn =
                    CS_storagePut( storage, key, stored[i]->buffer, stored[i]->size );
                CS_FAIL_ON_NULL( putIn, CS_tempBuffSnprintf(64, "Adding %s",key), "Failed." );
                if( putIn ) CS_storageReturnItem( putIn );
            }
            //Modify the sources
            for( int i = 0; i < NUM_THINGS; ++i ) {
                while( !compareThing( stored[i],stored2[i] ) ) modifyThing( stored[i] );
            }
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *taken = CS_storageGet( storage, key );
                CS_FAIL_ON_NULL( taken, CS_tempBuffSnprintf(64, "Taking %s",key), "Failed." );
                if( taken ) {
                    CS_FAIL_ON_FALSE( compareThingToStorageItem( stored[i],taken ), CS_tempBuffSnprintf(64, "Comparing modified original %s", key), "They ended up the same?" );
                    CS_FAIL_ON_TRUE( compareThingToStorageItem( stored2[i],taken ), CS_tempBuffSnprintf(64, "Comparing against copy of original %s", key), "They ended up different?" );
                    CS_storageReturnItem( taken );
                }
            }
        }
    }

    for( int i = 0; i < NUM_THINGS; ++i ) {
        if( stored ) freeThing(stored[i]);
        if( stored2 ) freeThing(stored2[i]);
    }
    if(stored)CS_free(stored);
    if(stored2)CS_free(stored2);
    CS_storageClose( storage );
    CS_storageTeardown();
    return testCount !=
           testSucceeded;
}


