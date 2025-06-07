#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/storage.h>

extern bool util_test_generic_storage(const struct CS_Storage * storage);

static int testCount = 0;
static int testSucceeded = 0;

#define NUM_THINGS 200
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

bool util_test_generic_storage(const struct CS_Storage * storage) {
    //Reset the counts
    testCount = 0;
    testSucceeded = 0;
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
        CS_FAIL_ON_TRUE( bError, "Allocate test data.", "Failed." );
        if( bError ) {
            for( int i = 0; i < NUM_THINGS; ++i ) {
                freeThing( stored[ i ] );
                stored[ i ] = NULL;
            }
        } else {
            //Store everything.
            struct CS_StorageItem *alreadyThere;
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *putIn =
                    CS_storagePut( storage, key, stored[i]->buffer, stored[i]->size, 0, NULL );
                CS_FAIL_ON_NULL( putIn, CS_tempBuffSnprintf(64, "Adding 'Index %d'",i), "Failed." );
                if( putIn ) CS_storageReturnItem( putIn );
            }
            //Modify the sources
            for( int i = 0; i < NUM_THINGS; ++i ) {
                while( !compareThing( stored[i],stored2[i] ) ) modifyThing( stored[i] );
            }
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *putInFalse =
                    CS_storagePut( storage, key, stored[0]->buffer, stored[0]->size, 0, &alreadyThere  );
                CS_FAIL_ON_NOT_NULL( putInFalse, CS_tempBuffSnprintf( 64, "Trying to put '%s' in again.", key), "Should have failed." );
                CS_FAIL_ON_NULL( alreadyThere, CS_tempBuffSnprintf( 64, "We should have a pointer to the original '%s'", key), "This is null..bad.");
                CS_FAIL_ON_FALSE( strcmp(alreadyThere->key, key) == 0, CS_tempBuffSnprintf( 64, "Returned item should be keyed '%s'", key), "Item key was %s", alreadyThere->key );
                if( putInFalse ) {
                    CS_storageReturnItem( putInFalse );
                    putInFalse = NULL;
                }
                if( alreadyThere ) {
                    CS_storageReturnItem( alreadyThere );
                    alreadyThere = NULL;
                }
            }
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *taken = CS_storageGet( storage, key );
                CS_FAIL_ON_NULL( taken, CS_tempBuffSnprintf(64, "Taking %s",key), "Failed." );
                if( taken ) {
                    CS_FAIL_ON_FALSE( compareThingToStorageItem( stored[i],taken ), CS_tempBuffSnprintf(64, "Comparing modified original %s", key), "They ended up the same?" );
                    CS_FAIL_ON_TRUE( compareThingToStorageItem( stored2[i],taken ), CS_tempBuffSnprintf(64, "Comparing against copy of original %s", key), "They ended up different?" );
                    CS_FAIL_ON_NULL( CS_storageItemChangeData( taken, stored[i]->size, 0, stored[i]->buffer), CS_tempBuffSnprintf(64, "Setting the data on '%s' to new modified data.", key), "Failed." );
                    struct CS_StorageItem *updated = CS_storageUpdate( storage, taken );
                    CS_FAIL_ON_TRUE( updated != taken, CS_tempBuffSnprintf( 64, "Update '%s'", key ), "Failed to update." );
                    if( updated != taken ) CS_storageReturnItem( updated );
                    CS_storageReturnItem( taken );
                    CS_FAIL_ON_NULL( taken = CS_storageGet( storage, key ), CS_tempBuffSnprintf(64, "Getting '%s' again", key), "Failed" );
                    if( taken ) {
                        CS_FAIL_ON_FALSE( compareThingToStorageItem( stored2[i],taken ), CS_tempBuffSnprintf(64, "Comparing '%s' to unmodified data.", key), "They ended up the same?" );
                        CS_FAIL_ON_TRUE( compareThingToStorageItem( stored[i],taken ), CS_tempBuffSnprintf(64, "Comparing '%s' to modified data.", key), "They ended up different?" );
                        CS_storageReturnItem( taken );
                    }
                }
            }

            //Remove everything.
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf(64, "Index %d", i);
                CS_FAIL_ON_TRUE( CS_storageRemove( storage, key ), CS_tempBuffSnprintf( 64, "Removing '%s'", key), "Failed." );
            }
            //Try to remove it a 2nd time
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf(64, "Index %d", i);
                CS_FAIL_ON_FALSE( CS_storageRemove( storage, key ), CS_tempBuffSnprintf( 64, "Removing '%s'", key), "Failed." );
            }

            //Re adding everything with a timeout in the past.
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *putIn =
                    CS_storagePut( storage, key, stored[i]->buffer, stored[i]->size, 1, NULL );
                CS_FAIL_ON_NULL( putIn, CS_tempBuffSnprintf(64, "Adding 'Index %d' past timeout",i), "Failed." );
                if( putIn ) CS_storageReturnItem( putIn );
            }
            //Putting it in a second time should succeed...
            for( int i = 0; i < NUM_THINGS; ++i ) {
                char *key = CS_tempBuffSnprintf( 64, "Index %d", i );
                struct CS_StorageItem *putIn =
                    CS_storagePut( storage, key, stored[i]->buffer, stored[i]->size, 1, NULL );
                CS_FAIL_ON_NULL( putIn, CS_tempBuffSnprintf(64, "Adding 'Index %d' past timeout 2",i), "Failed." );
                if( putIn ) CS_storageReturnItem( putIn );
            }
        }
    }

    for( int i = 0; i < NUM_THINGS; ++i ) {
        if( stored ) freeThing(stored[i]);
        if( stored2 ) freeThing(stored2[i]);
    }
    if(stored)CS_free(stored);
    if(stored2)CS_free(stored2);
    return testCount !=
           testSucceeded;
}


