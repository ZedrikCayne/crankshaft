#include <stdlib.h>
#include <stdio.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

extern bool test_test();

static int testCount = 0;
static int testSucceeded = 0;

bool test_test() {
#ifndef CS_TEST_SKIP_TESTTEST
    CS_LOG_LOUD("This is going to cause some explicit tests to fail. The overall test of testing won't because we are manipulating the test counts and successes internally. This is testing whether or not the test suite itself is doing what we expect it to do. If you don't want to see this make sure you define CS_TEST_SKIP_TESTTEST during compilation. (It should be in the Makefile already.)");
    bool fullOnFail = false;
    CS_FAIL_ON_TRUE(false,"Truthy check should pass","Oops!");
    if( testCount != testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_TRUE(true,"Truthy check should fail","This is supposed to fail!");
    if( testCount == testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_FALSE(false,"Truthy check should fail","This is supposed to fail!");
    if( testCount == testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_FALSE(true,"Truthy check should pass","Oops!");
    if( testCount != testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_NULL(NULL,"Null Check should fail","This is supposed to fail!");
    if( testCount == testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_NOT_NULL(NULL,"Null Check should pass","Oops!");
    if( testCount != testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_NULL((void*)15,"Null Check should pass","Oops!");
    if( testCount != testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    CS_FAIL_ON_NOT_NULL((void*)15,"Null Check should fail","This is supposed to fail!");
    if( testCount == testSucceeded ) fullOnFail = true; testSucceeded = testCount;
    return fullOnFail;
#endif
    return testCount != testSucceeded;
}


