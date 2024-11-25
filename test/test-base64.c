#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftrandom.h"

#include "crankshaftbase64.h"

extern bool test_base64(void);

static int testCount = 0;
static int testSucceeded = 0;

static char *testStrings[4] = {
    "The quick brown fox jumped over the lazy dog.",
    "The quick brown fox jumped over the lazy dog..",
    "The quick brown fox jumped over the lazy dog...",
    "The quick brown fox jumped over the lazy dog....",
};

#define FUZZ_BUFF_COUNT 2000
#define FUZZ_BUFF_SIZE (FUZZ_BUFF_COUNT * sizeof(int))

bool test_base64(void) {
    //Tests go here:
    int outLength;
    int outLength2;
    for( int i = 0; i < sizeof( testStrings ) / sizeof( testStrings[0] ); ++i ) {
        char *t0 = CS_base64EncodeTemp( testStrings[i], strlen(testStrings[i]) + 1, &outLength );
        CS_FAIL_ON_NULL( t0, CS_tempBuffSnprintf( 512, "base64 encode '%s'", testStrings[i] ), "Failed." );
        if( t0 ) {
            char *t1 = CS_base64DecodeTemp( t0, outLength, &outLength2 );
            CS_FAIL_ON_NULL( t1, CS_tempBuffSnprintf( 512, "base64 decode '%s'", t1 ), "Failed." );
            if( t1 ) {
                CS_FAIL_ON_FALSE( strcmp( testStrings[i], t1 ) == 0, "Strings match.", "'%s' vs '%s'", testStrings[i], t1 );
            }
        }
    }

    int *fuzzBuff = CS_alloc( FUZZ_BUFF_SIZE );
    CS_FAIL_ON_NULL( fuzzBuff, "Fuzz Buff Allocation.", "Failed to allocate the fuzz buff." );
    if( fuzzBuff ) {
        //Setup an initialstte for the fuzz buff...we're going mess with it incrementally
        for( int j = 0; j < FUZZ_BUFF_COUNT; ++j ) {
            fuzzBuff[ j ] = CS_testRand();
        }
        for( int i = 0; i < 50; ++i ) {
            for( int j = CS_testRandMax( 4 ); j < FUZZ_BUFF_COUNT; j += CS_testRandMax( 4 ) ) {
                fuzzBuff[ j ] = CS_testRand();
            }
            int inputSize = CS_testRandMax( FUZZ_BUFF_SIZE );
            int encodedSize;
            int decodedSize;
            char *t0 = CS_base64EncodeTemp( fuzzBuff, inputSize, &encodedSize );
            CS_FAIL_ON_NULL( t0, CS_tempBuffSnprintf( 64, "Encoding fuzz buffer #%d", i ), "Failed to encode fuzz buffer" );
            if( t0 ) {
                char *t1 = CS_base64DecodeTemp( t0, encodedSize, &decodedSize );
                CS_FAIL_ON_NULL( t1, CS_tempBuffSnprintf( 64, "Failed to decode fuzz buffer #%d", i ), "%s", t0 );
                CS_FAIL_ON_FALSE( memcmp( fuzzBuff, t1, inputSize ) == 0, CS_tempBuffSnprintf( 64, "Comparing fuzz buffer #%d", i ), "Not binary identical." );
            }
        }
    }

    return testCount !=
           testSucceeded;
}


