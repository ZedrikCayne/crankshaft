#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftalloc.h"

extern bool test_alloc(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_alloc(void) {
    //Tests go here:
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
    CS_FAIL_ON_NOT_NULL( (temp=CS_alloc(100)), "Alloc 25 on max 50 bytes", "We failed to allocate 25 bytes when the max was set to 50");
    if( temp != NULL ) CS_free(temp);
    CS_setMaxAlloc(0);
    return testCount !=
           testSucceeded;
}


