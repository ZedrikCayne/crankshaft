#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/util.h>
#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/string.h>

extern bool test_string(void);

static int testCount = 0;
static int testSucceeded = 0;

static char *cStrings[] = {
    "abcdefgh",
    "abcdefgh",
    "bbcdefgh",
    "abcdefgi",
    "abcdefghi"
};

static char counting[] = "0........10........20........30........40........50........60........70.......80........90........00........10........20........30........40........";


const struct CS_String *newStrings[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


static bool sameSign( int32_t a, int32_t b ) {
    if( a == b ) return true;
    if( a > 0 && b > 0 ) return true;
    if( a < 0 && b < 0 ) return true;
    return false;
}

static bool sameOffset( const void *base1, const void *result1, const void *base2, const void *result2, int32_t *out1, int32_t *out2 ) {
    if( result1 == NULL && result2 == NULL ) return true;
    if( out1 ) *out1 = result1 - base1;
    if( out2 ) *out2 = result2 - base2;
    return (result1 - base1) == (result2 - base2);
}

static char *randomCbuff( int32_t mySize ) {
    char * returnValue = CS_allocZero( mySize + 8 ); 
    if( returnValue ) {
        for(int i=0; i < mySize; ++i) {
            returnValue[i] = (char)('a' + CS_testRandMax(25));
        }
    }
    return returnValue;
}

bool test_string(void) {
    //Tests go here:
    int nLen = CS_ARRAY_SIZE(cStrings);

    for( int i = 0; i < nLen; ++i ) {
        newStrings[ i ] = CS_stringCopyCstring( cStrings[ i ], -1 );
    }

    for( int i = 0; i < nLen; ++i ) {
        for( int j = 0; j < nLen; ++j ) {
            int cResult = strcmp(cStrings[i],cStrings[j]);
            int sResult = CS_stringStrncmp(newStrings[i], newStrings[j], -1);
            int mcResult = strncmp(cStrings[i], cStrings[j], 8);
            int msResult = CS_stringStrncmp(newStrings[i], newStrings[j], 8 );
            CS_FAIL_ON_FALSE( sameSign(cResult,sResult), "CS_stringStrncmp(-1)", "Failed on %d %d with %d, %d", i, j, cResult, sResult );
            CS_FAIL_ON_FALSE( sameSign(mcResult, msResult), "CS_stringStrncmp(8)", "Failed on %d %d with %d %d", i, j, mcResult, msResult );
        }
    }


    int nStrings = CS_testRandMax( 50 ) + 10;
    char **randCStrings = CS_allocZero( sizeof (char*) * nStrings );
    const struct CS_String **randStrings = CS_allocZero( sizeof( struct CS_String * ) * nStrings );

    for( int i = 0; i < nStrings; ++i ) {
        int stringLength = CS_testRandMax( 120 ) + 22;
        int startingOffset = CS_testRandMax( stringLength - 22 );
        randCStrings[i] = randomCbuff( stringLength );
        memcpy( randCStrings[i] + startingOffset, cStrings[ CS_testRandMax(nLen - 1) ], 8 );
        randStrings[ i ] = CS_stringCopyCstring( randCStrings[i], -1 );
    }

    for( int i = 0; i < nStrings; ++i ) {
        for( int j = 0; j < nStrings; ++j ) {
            if( i == j ) continue;
            int cResult = strcmp(randCStrings[i],randCStrings[j]);
            int sResult = CS_stringStrncmp(randStrings[i], randStrings[j], -1);
            CS_FAIL_ON_FALSE( sameSign(cResult,sResult), "CS_stringStrncmp(-1)", "Failed on %d %d with %d, %d", i, j, cResult, sResult );

        }
        for( int j = 0; j < nLen; ++j ) {
            char * result = strstr( randCStrings[i], cStrings[j] );
            const char * reResult = CS_stringStrstr( randStrings[i], newStrings[j] );
            int32_t out1, out2;
            CS_FAIL_ON_FALSE( sameOffset(randCStrings[i], result, randStrings[i]->data, reResult, &out1, &out2 ), "CS_stringStrstr()", "Failed on %d %d with %d %d", i, j, out1, out2 );
        }
    }

    for( int i = 0; i < nLen; ++i ) {
        CS_stringFree( newStrings[i] );
        newStrings[ i ] = NULL;
    }
    
    for( int i = 0; i < nStrings; ++i ) {
        CS_stringFree( randStrings[i] );
        CS_free( randCStrings[i] );
    }

    return testCount !=
           testSucceeded;
}


