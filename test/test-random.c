#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/random.h>
#include <stdint.h>

extern bool test_random(void);

static int32_t testCount = 0;
static int32_t testSucceeded = 0;

#define NUM_RANDOM_INTS 789


bool test_random(void) {
    //Tests go here:
    struct CS_LCG_rand_state testState = {0};
    int32_t newSeed = CS_testRand();

    int32_t *testBuff1 = CS_alloc( sizeof( int32_t ) * NUM_RANDOM_INTS );
    int32_t i;

    CS_FAIL_ON_FALSE( (testBuff1 != NULL), "Allocating memory for random tests.", "Got null on one of our test buffers, bailing." );

    if( testBuff1 ) {
        CS_LCG_rand_init( &testState, newSeed );
        for( i = 0; i < NUM_RANDOM_INTS; ++i ) {
            testBuff1[i] = CS_LCG_rand( &testState );
            CS_LOG_TRACE("Rand %d", testBuff1[i]);
            if( i > 0 ) {
                CS_FAIL_ON_TRUE( testBuff1[i] == testBuff1[i-1], "Consecutive random numbers not equal.", "Oops, equal." );
            }
        }
        CS_LCG_rand_init( &testState, newSeed );
        for( i = 0; i < NUM_RANDOM_INTS; ++i ) {
            char * tBuff = CS_tempBuff( 256 );
            snprintf( tBuff, 256, "Random #%d", i );
            CS_FAIL_ON_FALSE( CS_LCG_rand( &testState ) == testBuff1[i], tBuff, "Did not match." );
        }
        int32_t counters[6] = {0};
        int32_t totalRolled = 0;
        for( i = 0; i < NUM_RANDOM_INTS; ++i ) {
            int32_t a = testBuff1[i] & 0x7;
            if( a < 6 ) {
                counters[a]++;
                totalRolled++;
            }
        }
        double result = counters[0]*1 +
                        counters[1]*2 + 
                        counters[2]*3 +
                        counters[3]*4 +
                        counters[4]*5 +
                        counters[5]*6;
        result = result / (double)totalRolled;
        CS_LOG_TRACE("Rand: %d rolls. %d %d %d %d %d %d. Average %f", totalRolled,
                counters[0], counters[1], counters[2], counters[3],
                counters[4], counters[5], result);
        CS_FAIL_ON_FALSE( fabs(result - 3.5) < 0.03, "Random dice rolls should be ok.", "Average is off.");
    }

    if( testBuff1 ) CS_free( testBuff1 );

    return testCount !=
           testSucceeded;
}


