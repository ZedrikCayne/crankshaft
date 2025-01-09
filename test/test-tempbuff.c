#include <stdlib.h>
#include <stdio.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/tempbuff.h>

extern bool test_tempbuff(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_tempbuff(void) {
    bool returnValue = false;

    void *tempBuff;
    CS_FAIL_ON_NULL( tempBuff = CS_tempAllocManual("TEST1", 16),"Allocating temp buffer","Failed to allocate a temp buffer");
    if( tempBuff ) {
        void *t1 = CS_tempGetManual( tempBuff, 5 );
        void *t2 = CS_tempGetManual( tempBuff, 5 );
        CS_FAIL_ON_TRUE(((char *)t1 + 8 != (char *)t2),"Pointers on a temp buff of size 5 with alignment of 8 bytes should be 8 bytes apart.","We got %p and %p",t1,t2);
        void *t3 = CS_tempGetManual( tempBuff, 5 );
        CS_FAIL_ON_TRUE((t3 != t1),"The third pointer taken from a temp buff of size 16 should be the same as the first pointer.","We got %p and %p", t1, t3 );
        CS_tempFreeManual( tempBuff );
    }

    CS_FAIL_ON_NULL(tempBuff = CS_tempAllocManual("TEST2",24),"Create a temp buff with alignment 1","Got NULL!");
    if( tempBuff ) {
        void *t1 = CS_tempGetManual( tempBuff, 9 );
        void *t2 = CS_tempGetManual( tempBuff, 8 );
        CS_FAIL_ON_TRUE((char *) t1 + 16 != (char *)t2, "Pointers on a temp buff of size 24 alignment 8 should be 16 bytes apart.", "We got %p and %p", t1, t2);
        void *t3 = CS_tempGetManual( tempBuff, 8 );
        CS_FAIL_ON_TRUE((t3 != t1),"The third pointer taken from a temp buff of size 2 should be the same as the first pointer.","We got %p and %p", t1, t3 );
        CS_tempFreeManual( tempBuff );
    }

    CS_FAIL_ON_NOT_NULL(tempBuff = CS_tempAllocManual("TESTBUFFERWITHMUCHTOOLONGNAMEYEAHMAXIS64123456789012345678980123456", 32), "Temp buff names have a max user defined length of 64", "We should not have been able to create one.");
    if( tempBuff != NULL ) CS_tempFreeManual( tempBuff );
    
    CS_FAIL_ON_NOT_NULL(tempBuff = CS_tempAllocManual("TEST3", -32 ), "Temp buff sizes have to be positive.", "We should not be able to create one." );
    if( tempBuff != NULL ) CS_tempFreeManual( tempBuff );

    CS_FAIL_ON_NULL(tempBuff = CS_tempAllocManual("", 32 ), "Temp buffs can have an empty string for their name.", "Got NULL");
    if( tempBuff != NULL ) CS_tempFreeManual( tempBuff );

    CS_FAIL_ON_NOT_NULL(tempBuff = CS_tempAllocManual(NULL, 32 ), "Temp buffs must have a name, not NULL.", "We should not be able to create one.");
    if( tempBuff != NULL ) CS_tempFreeManual( tempBuff );

    return returnValue;
}

