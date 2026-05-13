#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <stdint.h>

extern bool test_test2(void);

static int32_t testCount = 0;
static int32_t testSucceeded = 0;

bool test_test3(void) {
    //Tests go here:
#ifndef CS_TEST_SKIP_TESTTEST
    int32_t currentTestCount = testCount;
    int32_t currentTestSucceeded = testSucceeded;

    CS_FAIL_ON_FALSE( currentTestCount == 0, "Test count not affected by any other tests.", "Test count not 0 on the way in." );
    CS_FAIL_ON_FALSE( currentTestSucceeded == 0, "Test successes not affected by any other tests.", "Test success not 0 on the way in." );

#endif
    return testCount !=
           testSucceeded;
}


