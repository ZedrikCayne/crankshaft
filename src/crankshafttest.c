#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/random.h>
#include <crankshaft/alloc.h>

extern bool TEST_AUTO(void);

static struct CS_LCG_rand_state testRandState = {0};

bool CS_TEST_PRINT_ONLY_ERRORS = false;
int CS_TEST_seed = 0;

bool CS_testMain(void) {
    if( CS_TEST_seed == 0 ) {
        CS_srand(time(NULL));
        CS_TEST_seed = CS_rand();
    }
    int allocSystemError = CS_allocSystemTracker(5000,CS_ALLOC_FLAG_ALL);
    if( allocSystemError != 0 ) CS_LOG_LOUD("Alloc tracking system not working.");
    CS_tempAllocateGlobal(5*1024*1024);
    CS_LOG_LOUD("Test random seed is %d, call CS_testSetRandomSeed() to set it explicitly or use --seed if you are using the included main.cpp for repeatable tests in the future.", CS_TEST_seed);

    bool returnValue = TEST_AUTO();
    CS_allocSystemReport();
    CS_tempFreeGlobal();
    allocSystemError = CS_allocSystemTrackerKill();
    if( allocSystemError != 0 ) CS_LOG_LOUD("Alloc tracking failed to de-init.");
    return returnValue;
}

void CS_testSetRandomSeed(int seed) {
    CS_TEST_seed = seed;
}

void CS_testResetGlobalRandSeed() {
    CS_LCG_rand_init( &testRandState, CS_TEST_seed );
}

int CS_testRand() {
    return CS_LCG_rand(&testRandState);
}

int CS_testRandMax(int max) {
    int returnValue = -1;
    int mask = max + 1;
    int add = mask >> 1;
    while( add > 0 ) {
        mask |= add;
        add = add >> 1;
    }
    while( (returnValue = (CS_LCG_rand(&testRandState)&mask)) > max );
    return returnValue;
}

#ifndef CS_AUTOTEST_ENABLED
bool TEST_AUTO(void) {
    CS_LOG_LOUD("No tests defined");
    return false;
}
#endif

