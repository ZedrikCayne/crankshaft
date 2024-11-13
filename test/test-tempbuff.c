#include <stdlib.h>
#include <stdio.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshafttempbuff.h"

extern bool test_tempbuff(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_tempbuff(void) {
    bool returnValue = false;

    void *tempBuff;
    CS_FAIL_ON_NULL( tempBuff = CS_allocManualTempBuff("TEST1", 5, 2, 4),"Allocating temp buffer","Failed to allocate a temp buffer");
    if( tempBuff ) {
        void *t1 = CS_getTempBuff( tempBuff );
        void *t2 = CS_getTempBuff( tempBuff );
        CS_FAIL_ON_TRUE(((char *)t1 + 8 != (char *)t2),"Pointers on a temp buff of size 5 with alignment of 4 bytes should be 8 bytes apart.","We got %p and %p",t1,t2);
        void *t3 = CS_getTempBuff( tempBuff );
        CS_FAIL_ON_TRUE((t3 != t1),"The third pointer taken from a temp buff of size 2 should be the same as the first pointer.","We got %p and %p", t1, t3 );
        CS_freeManualTempBuff( tempBuff );
    }

    CS_FAIL_ON_NULL(tempBuff = CS_allocManualTempBuff("TEST2",5,2,1),"Create a temp buff with alignment 1","Got NULL!");
    if( tempBuff ) {
        void *t1 = CS_getTempBuff( tempBuff );
        void *t2 = CS_getTempBuff( tempBuff );
        CS_FAIL_ON_TRUE((char *) t1 + 5 != (char *)t2, "Pointers on a temp buff of size 5 alignment 1 should be 5 bytes apart.", "We got %p and %p", t1, t2);
        void *t3 = CS_getTempBuff( tempBuff );
        CS_FAIL_ON_TRUE((t3 != t1),"The third pointer taken from a temp buff of size 2 should be the same as the first pointer.","We got %p and %p", t1, t3 );
        CS_freeManualTempBuff( tempBuff );
    }

    CS_FAIL_ON_NOT_NULL(tempBuff = CS_allocManualTempBuff("TESTBUFFERWITHMUCHTOOLONGNAMEYEAHMAXIS64123456789012345678980123456", 5, 2, 4), "Temp buff names have a max user defined length of 64", "We should not have been able to create one.");
    if( tempBuff != NULL ) CS_freeManualTempBuff( tempBuff );
    
    CS_FAIL_ON_NOT_NULL(tempBuff = CS_allocManualTempBuff("TEST3", -5, 2, 4 ), "Temp buff sizes have to be positive.", "We should not be able to create one." );
    if( tempBuff != NULL ) CS_freeManualTempBuff( tempBuff );

    CS_FAIL_ON_NOT_NULL(tempBuff = CS_allocManualTempBuff("TEST3", 5, -2, 4 ), "Temp buff count has to be positive.", "We should not be able to create one." );
    if( tempBuff != NULL ) CS_freeManualTempBuff( tempBuff );

    CS_FAIL_ON_NOT_NULL(tempBuff = CS_allocManualTempBuff("TEST3", 5, 2, -4 ), "Temp buff alignments have to be positive.", "We should not be able to create one." );
    if( tempBuff != NULL ) CS_freeManualTempBuff( tempBuff );

    CS_FAIL_ON_NULL(tempBuff = CS_allocManualTempBuff("", 5, 2, 4 ), "Temp buffs can have an empty string for their name.", "Got NULL");
    if( tempBuff != NULL ) CS_freeManualTempBuff( tempBuff );

    CS_FAIL_ON_NOT_NULL(tempBuff = CS_allocManualTempBuff(NULL, 5, 2, 4 ), "Temp buffs must have a name, not NULL.", "We should not be able to create one.");
    if( tempBuff != NULL ) CS_freeManualTempBuff( tempBuff );

    return returnValue;
}

