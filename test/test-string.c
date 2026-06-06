#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <crankshaft/util.h>
#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/string.h>
#include <stdint.h>

extern bool test_string(void);

static int32_t testCount = 0;
static int32_t testSucceeded = 0;

static char *cStrings[] = {
    "",
    "",
    "abcdefgh",
    "abcdefgh",
    "bbcdefgh",
    "abcdefgi",
    "abcdefghi"
};

static struct CS_String toTokenize = CS_STRING("g_state={\"i_l\":0,\"i_ll\":1779131810435,\"i_e\":{\"enable_itp_optimization\":0},\"i_et\":0000000000083}; new_session_key=01234567-89ab-cdef-fedc-ba9876543210; third=wakka ;trim");

static const struct CS_String toTokenizeResults[] = {
    CS_STRING("g_state={\"i_l\":0,\"i_ll\":1779131810435,\"i_e\":{\"enable_itp_optimization\":0},\"i_et\":0000000000083}"),
    CS_STRING(" new_session_key=01234567-89ab-cdef-fedc-ba9876543210"),
    CS_STRING(" third=wakka"),
    CS_STRING("; third=wakka")
};


const struct CS_String *newStrings[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
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
        for(int32_t i=0; i < mySize; ++i) {
            returnValue[i] = (char)('a' + CS_testRandMax(25));
        }
    }
    return returnValue;
}

bool test_string(void) {
    //Tests go here:
    int32_t nLen = CS_ARRAY_SIZE(cStrings);

    for( int32_t i = 0; i < nLen; ++i ) {
        newStrings[ i ] = CS_stringCopyCstring( cStrings[ i ], -1 );
    }

    for( int32_t i = 0; i < nLen; ++i ) {
        for( int32_t j = 0; j < nLen; ++j ) {
            int32_t cResult = strcmp(cStrings[i],cStrings[j]);
            int32_t sResult = CS_stringStrncmp(newStrings[i], newStrings[j], -1);
            int32_t mcResult = strncmp(cStrings[i], cStrings[j], 8);
            int32_t msResult = CS_stringStrncmp(newStrings[i], newStrings[j], 8 );
            CS_FAIL_ON_FALSE( sameSign(cResult,sResult), "CS_stringStrncmp(-1)", "Failed on %d %d with %d, %d", i, j, cResult, sResult );
            CS_FAIL_ON_FALSE( sameSign(mcResult, msResult), "CS_stringStrncmp(8)", "Failed on %d %d with %d %d", i, j, mcResult, msResult );
        }
    }


    int32_t nStrings = CS_testRandMax( 50 ) + 10;
    char **randCStrings = CS_allocZero( sizeof (char*) * nStrings );
    const struct CS_String **randStrings = CS_allocZero( sizeof( struct CS_String * ) * nStrings );

    for( int32_t i = 0; i < nStrings; ++i ) {
        int32_t stringLength = CS_testRandMax( 120 ) + 22;
        int32_t startingOffset = CS_testRandMax( stringLength - 22 );
        randCStrings[i] = randomCbuff( stringLength );
        memcpy( randCStrings[i] + startingOffset, cStrings[ CS_testRandMax(nLen - 1) ], 8 );
        randStrings[ i ] = CS_stringCopyCstring( randCStrings[i], -1 );
    }

    for( int32_t i = 0; i < nStrings; ++i ) {
        for( int32_t j = 0; j < nStrings; ++j ) {
            if( i == j ) continue;
            int32_t limit = CS_testRandMax(20) + 10;
            int32_t cResult = strcmp(randCStrings[i],randCStrings[j]);
            int32_t c2Result = strncmp(randCStrings[i],randCStrings[j],limit);
            int32_t sResult = CS_stringStrncmp(randStrings[i], randStrings[j], -1);
            int32_t s2Result = CS_stringStrncmp(randStrings[i], randStrings[j],limit);
            CS_FAIL_ON_FALSE( sameSign(cResult,sResult), "CS_stringStrncmp(-1)", "Failed on %d %d with %d, %d", i, j, cResult, sResult );
            CS_FAIL_ON_FALSE( sameSign(cResult,sResult), "CS_stringStrncmp(n)", "Failed on %d %d %d with %d, %d", i, j, limit, c2Result, s2Result );

        }
        for( int32_t j = 0; j < nLen; ++j ) {
            char * result = strstr( randCStrings[i], cStrings[j] );
            const struct CS_String* reResult = CS_stringStrstr( randStrings[i], newStrings[j] );
            int32_t out1, out2;
            CS_FAIL_ON_FALSE( sameOffset(randCStrings[i], result, randStrings[i]->data, reResult?reResult->data:NULL, &out1, &out2 ), "CS_stringStrstr()", "Failed on %d %d with %d %d", i, j, out1, out2 );
            CS_stringFree(reResult);
        }
    }

    for( int32_t i = 0; i < nLen; ++i ) {
        CS_stringFree( newStrings[i] );
        newStrings[ i ] = NULL;
    }
    
    for( int32_t i = 0; i < nStrings; ++i ) {
        CS_stringFree( randStrings[i] );
        CS_free( randCStrings[i] );
    }

    const struct CS_String *tokenReturn;
    const char *saveptr = NULL;

    toTokenize.length -= 6; //Removing " ;trim"

    int numTokens = 0;

    while( (tokenReturn = CS_stringTempStrtok( &toTokenize, &CS_STRING(";"), &saveptr )) ) {
        CS_FAIL_ON_FALSE( CS_stringStrcmp( toTokenizeResults + numTokens, tokenReturn ) == 0, "Check strtok output", "Failed." );
        ++numTokens;
    }

    CS_FAIL_ON_FALSE( numTokens == 3, "Check number of tokens parsed.", "Wanted 3 got %d", numTokens ); 

    tokenReturn = CS_stringStrrstr( &toTokenize, &CS_STRING(";") );
    CS_FAIL_ON_FALSE( tokenReturn && CS_stringStrcmp( toTokenizeResults + 3, tokenReturn ) == 0, "Check Strrstr result", "Failed." );
    if( tokenReturn ) CS_stringFree(tokenReturn);


    return testCount !=
           testSucceeded;
}


