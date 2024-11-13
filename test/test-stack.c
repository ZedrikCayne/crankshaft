#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

#include "crankshaftstack.h"

extern bool test_stack();

static int testCount = 0;
static int testSucceeded = 0;

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#define STACK_SIZE 32

bool test_stack() {
    //Tests go here:

    struct CS_Stack *t0 = CS_Stack_allocPointer(STACK_SIZE);

    char __temp[ 1024 ];

    void *__randy[ STACK_SIZE * 4 ];

    unsigned long long i;

    for( i = 0; i < STACK_SIZE * 4; ++i ) {
        __randy[ i ] = (void*)CS_testRand();
    }

    CS_FAIL_ON_NULL(t0, "Creating a stack", "Got a null." );
    if( t0 ) {
        for( i = 0; i < STACK_SIZE * 3; ++i ) {
            snprintf( __temp, 1024, "Push %p", __randy[i] );
            CS_FAIL_ON_TRUE( CS_pushPointer( t0, __randy[i] ), __temp, "Failed to push %p", __randy[ i ] );
        }
         for( i = 0; i < STACK_SIZE * 3; ++i ) {
            snprintf( __temp, 1024, "Pop %p", __randy[ STACK_SIZE * 3 - i - 1 ] );
            void *popped = CS_popPointer( t0 );
            CS_FAIL_ON_TRUE( popped != __randy[ STACK_SIZE * 3 - i - 1 ], __temp, "Failed to pop %p : got %p", __randy[ STACK_SIZE * 3 - i - 1 ], popped );
        }
        CS_Stack_free(t0);
    }



    return testCount !=
           testSucceeded;
}


