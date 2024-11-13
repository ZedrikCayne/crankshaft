#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

extern bool test_testfail(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_testfail(void) {
    //Tests go here:
#ifndef CS_TEST_SKIP_TESTTEST
    CS_LOG_LOUD("This is going to cause the tests to fail, and is just meant to make sure that indeed, if a test fails somewhere along the line, the system does indeed return a fail to the user when one runs the tests. If you don't want to see this make sure you define CS_TEST_SKIP_TESTTEST during compilation. (It should be in the Makefile already.)");
    return testCount == testSucceeded;
#else
    return testCount != testSucceeded;
#endif
}


