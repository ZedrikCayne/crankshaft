#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <stdint.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/alloc.h>
#include <stdint.h>

extern bool test_alloc(void);

static int32_t testCount = 0;
static int32_t testSucceeded = 0;

bool test_alloc(void) {
    //Tests go here:
#ifndef CS_ALLOC_USE_MALLOC
    void *temp;
    CS_setFailAlloc(0);
    CS_FAIL_ON_NULL( (temp = CS_alloc(100)), "0% malloc fail", "We didn't get a null");
    if( temp != NULL ) CS_free(temp);
    CS_setFailAlloc(100);
    CS_FAIL_ON_NOT_NULL( (temp = CS_alloc(100)), "100% malloc fail", "We didn't get a null");
    if( temp != NULL ) CS_free(temp);
    CS_setFailAlloc(0);
    CS_setMaxAlloc(50);
    CS_FAIL_ON_NOT_NULL( (temp=CS_alloc(100)), "Alloc 100 on max 50 bytes", "We managed to allocate 100 bytes when the max was set to 50");
    if( temp != NULL ) CS_free(temp);
    CS_FAIL_ON_NULL( (temp=CS_alloc(25)), "Alloc 25 on max 50 bytes", "We failed to allocate 25 bytes when the max was set to 50");
    if( temp != NULL ) CS_free(temp);
    CS_setMaxAlloc(0);
#endif
    return testCount !=
           testSucceeded;
}


