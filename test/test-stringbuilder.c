#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

#include "crankshaftstringbuilder.h"

extern bool test_stringbuilder(void);

static int testCount = 0;
static int testSucceeded = 0;

const char *tenChar = "1234567890";
const char *twentyTwoChar = "1234567890123456789012";
const char *thirtyTwoChar = "12345678901234567890123456789012";
const char *quickBrownFox = "The quick brown fox jumps over the lazy dog.";

#define ORIGINAL_SIZE 32
#define LENGTH_OF_TENCHAR 10
#define LENGTH_OF_TWENTYTWO 22
#define LENGTH_OF_THIRTYTWO 32

bool test_stringbuilder(void) {
    //Tests go here:

    struct CS_StringBuilder *sb = CS_SB_create( ORIGINAL_SIZE );

    CS_FAIL_ON_NULL( sb, "Initial allocaiton of a string builder.", "Got NULL" );

    if( sb ) {
        CS_FAIL_ON_NULL( CS_SB_append( sb, tenChar ), "Appending ten to empty.", "Got NULL." );
        CS_FAIL_ON_FALSE( sb && memcmp( sb->buffer, tenChar, LENGTH_OF_TENCHAR ) == 0, "Checking...", "Not identical." );
        CS_FAIL_ON_NULL( CS_SB_append( sb, twentyTwoChar ), "Appending twenty two to ten.", "Got NULL" );
        CS_FAIL_ON_FALSE( sb && memcmp( sb->buffer, thirtyTwoChar, LENGTH_OF_THIRTYTWO ) == 0, "Checking...", "Not identical." );
        CS_FAIL_ON_FALSE( sb && sb->currentSize == (sb->originalSize + ORIGINAL_SIZE), "Checking expansion example.", "Size unexpected. %s", CS_SB_desc(sb) );
        CS_FAIL_ON_NULL( CS_SB_printf( sb, "%s%s", thirtyTwoChar, quickBrownFox ), "Append a lot.", "Oops! %s", CS_SB_desc(sb) );
    }


    if( sb ) CS_SB_free(sb);

    return testCount !=
           testSucceeded;
}


