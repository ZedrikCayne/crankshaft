#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftlinearalloc.h"

extern bool test_linearalloc(void);

static int testCount = 0;
static int testSucceeded = 0;

#define SIZE_OF_ALLOCATOR 256

bool test_linearalloc(void) {
    
    //Tests go here:
    void *voidAllocator;

    void *t1;
    void *t2;
    void *t3;
    void *t4;

    CS_FAIL_ON_NULL(voidAllocator = CS_allocLinearAllocator( SIZE_OF_ALLOCATOR ), "Allocating a linear allocator.", "Got NULL");

    t1 = CS_takeLinear(voidAllocator,0,1);
    t2 = CS_takeLinear(voidAllocator,0,1);
    CS_FAIL_ON_FALSE( t1 == t2, "Two 0 size allocations should be equal.", "Got %p and %p", t1, t2 );

    CS_resetLinear(voidAllocator);
    CS_FAIL_ON_NOT_NULL(t1 = CS_takeLinear(voidAllocator, SIZE_OF_ALLOCATOR + 1, 1), "Trying to allocate larger than the initial size should fail.", "Got %p", t1);
    CS_FAIL_ON_NULL(t1 = CS_takeLinear(voidAllocator, SIZE_OF_ALLOCATOR, 1), "Allocating exactly the large amount from a linear allocator.", "Got a NULL" );

    CS_resetLinear(voidAllocator);
    t1 = CS_takeLinear( voidAllocator, SIZE_OF_ALLOCATOR - 20, 1 );
    t2 = CS_takeLinear( voidAllocator, 0, 1 );
    t3 = CS_takeLinear( voidAllocator, SIZE_OF_ALLOCATOR, 1 );
    t4 = CS_takeLinear( voidAllocator, 20, 1 );

    CS_FAIL_ON_FALSE( t2 == t4, "A small allocation on the end of a partially allocated linear allocator slab should fit on the end.", "%p != %p", t2, t4 );

    CS_resetLinear(voidAllocator);
    CS_FAIL_ON_FALSE( t1 == CS_takeLinear( voidAllocator, SIZE_OF_ALLOCATOR - 20, 1 ), "Replay #1", "Values different." );
    CS_FAIL_ON_FALSE( t2 == CS_takeLinear( voidAllocator, 0, 1 ), "Replay #2", "Values different." );
    CS_FAIL_ON_FALSE( t3 == CS_takeLinear( voidAllocator, SIZE_OF_ALLOCATOR, 1 ), "Replay #3", "Values different." );
    CS_FAIL_ON_FALSE( t4 == CS_takeLinear( voidAllocator, 20, 1 ), "Replay #4", "Values different." );

    CS_resetLinear(voidAllocator);
    t1 = CS_takeLinear( voidAllocator, 13, 7 );
    t2 = CS_takeLinear( voidAllocator, 25, 1 );
    CS_FAIL_ON_FALSE( ((char*)t2 - (char*)t1) == 13, "Allocation of 13 bytes at 7 alignment then one of 25 at 1 should end up being 13 bytes off.", "%p %p", t1, t2);

    CS_resetLinear(voidAllocator);
    t1 = CS_takeLinear( voidAllocator, 1, 3 );
    t2 = CS_takeLinear( voidAllocator, 1, 3 );
    t3 = CS_takeLinear( voidAllocator, 1, 3 );
    t4 = CS_takeLinear( voidAllocator, 1, 3 );

    CS_FAIL_ON_FALSE( ((char*)t2 - (char*)t1) == 3, "Taking 1 byte alignment 3 should be 3 bytes off.", "%p %p", t1, t2 );
    CS_FAIL_ON_FALSE( ((char*)t3 - (char*)t2) == 3, "Taking 1 byte alignment 3 should be 3 bytes off.", "%p %p", t2, t3 );
    CS_FAIL_ON_FALSE( ((char*)t4 - (char*)t3) == 3, "Taking 1 byte alignment 3 should be 3 bytes off.", "%p %p", t3, t4 );

    CS_freeLinearAllocator(voidAllocator);

    return testCount !=
           testSucceeded;
}


