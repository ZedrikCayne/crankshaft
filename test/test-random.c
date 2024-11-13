#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftrandom.h"

extern bool test_random(void);

static int testCount = 0;
static int testSucceeded = 0;

#define NUM_RANDOM_INTS 789


bool test_random(void) {
    //Tests go here:
    struct CS_LCG_rand_state testState = {0};
    int newSeed = CS_testRand();

    int *testBuff1 = CS_alloc( sizeof( int ) * NUM_RANDOM_INTS );
    int i;

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
        int counters[6] = {0};
        int totalRolled = 0;
        for( i = 0; i < NUM_RANDOM_INTS; ++i ) {
            int a = testBuff1[i] & 0x7;
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


