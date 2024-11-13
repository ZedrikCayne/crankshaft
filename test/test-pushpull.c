#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftpushpull.h"
#include "crankshafttempbuff.h"

extern bool test_pushpull();

static int testCount = 0;
static int testSucceeded = 0;

#define TEST_BUFF_SIZE 256
#define TEST_READ_SIZE 55
#define TEST_WRITE_SIZE 46

bool test_pushpull() {
    //Tests go here:
    struct CS_PushPullBuffer *testBuffer1 = CS_PP_defaultAlloc( TEST_BUFF_SIZE );
    CS_FAIL_ON_NULL(testBuffer1,"Failed to allocate a buffer at all.", "Got NULL");
    CS_FAIL_ON_FALSE( CS_PP_startOfData(testBuffer1) == CS_PP_endOfData(testBuffer1), "Initial buffer start and end same.", "%p %p", CS_PP_startOfData(testBuffer1), CS_PP_endOfData(testBuffer1) );
    CS_FAIL_ON_FALSE( CS_PP_dataSize(testBuffer1) == 0, "Size of data should be 0", "%d", CS_PP_dataSize(testBuffer1) );
    CS_FAIL_ON_FALSE( CS_PP_bufferRemaining(testBuffer1) == TEST_BUFF_SIZE, "Buffer remain should be full.", "%d", CS_PP_bufferRemaining(testBuffer1) );
    CS_FAIL_ON_FALSE( CS_PP_read(testBuffer1, TEST_READ_SIZE) == TEST_READ_SIZE, "Read of size x returns x.", "Did not get same size.");
    CS_FAIL_ON_FALSE( CS_PP_dataSize(testBuffer1) == TEST_READ_SIZE, "Size in buffer should be what we read into it.", "Sizes did not match.");
    CS_FAIL_ON_FALSE( CS_PP_bufferRemaining(testBuffer1) == (TEST_BUFF_SIZE - TEST_READ_SIZE), "Buffer remaining should be what's left.", "Unexpected number." );
    CS_FAIL_ON_FALSE( CS_PP_write(testBuffer1, TEST_READ_SIZE ) == TEST_READ_SIZE, "Write of size x should return x.", "Unexpected number." );
    CS_FAIL_ON_FALSE( CS_PP_startOfData(testBuffer1) == testBuffer1->buff, "After reading the buffer empty...it should reset to 0.", "Start of data does not match head of buffer." );
    CS_PP_read(testBuffer1, TEST_READ_SIZE);
    CS_FAIL_ON_FALSE(CS_PP_write(testBuffer1, TEST_BUFF_SIZE) == TEST_READ_SIZE, "Trying to write more than what was in the buffer should only return number of bytes that were in the buffer.", "Unexpected number" );
    CS_FAIL_ON_FALSE(CS_PP_dataSize(testBuffer1) == 0, "Buffer should be empty.", "%s", CS_PP_desc(testBuffer1) );
    CS_FAIL_ON_FALSE(CS_PP_read(testBuffer1, TEST_BUFF_SIZE) == TEST_BUFF_SIZE, "Should be able to fill whole buffer.", "Did not write all bytes.");
    CS_FAIL_ON_FALSE(CS_PP_dataSize(testBuffer1) == TEST_BUFF_SIZE,"Size of full buffer should be same as initial size.", "Sizes do not match.");
    CS_FAIL_ON_FALSE(CS_PP_bufferRemaining(testBuffer1) == 0, "Buffer should nave 0 remain after being pushed full.", "%s", CS_PP_desc(testBuffer1) );
    CS_FAIL_ON_FALSE(CS_PP_write(testBuffer1, TEST_BUFF_SIZE) == TEST_BUFF_SIZE, "Writing out of a completely full buffer exact number of bytes should return same number of bytes.", "Sizes do not match.");
    CS_FAIL_ON_FALSE(CS_PP_dataSize(testBuffer1) == 0, "Size should be 0 after complete write.", "Size not zero.");
    CS_FAIL_ON_FALSE(CS_PP_read(testBuffer1, TEST_BUFF_SIZE * 2) == TEST_BUFF_SIZE, "Trying to push more data in should only return max size,", "Sizes do not match.");
    CS_FAIL_ON_FALSE(CS_PP_write(testBuffer1, TEST_WRITE_SIZE) == TEST_WRITE_SIZE, "Partial write should only return # bytes written.", "Sizes different.");
    CS_FAIL_ON_FALSE(CS_PP_write(testBuffer1, TEST_BUFF_SIZE) == TEST_BUFF_SIZE - TEST_WRITE_SIZE, "Write of rest of buffer should return only remaining size.", "Sizes differ.");
    CS_PP_reset(testBuffer1);
    CS_FAIL_ON_FALSE( CS_PP_dataSize( testBuffer1 ) == 0, "Reset buffer size of data should be 0", "Sizes don't match.");
    CS_FAIL_ON_FALSE( CS_PP_bufferRemaining( testBuffer1 ) == TEST_BUFF_SIZE, "Buffer remain on a reset buffer should be buffer size.", "Sizes differ." );
    CS_PP_read(testBuffer1, TEST_READ_SIZE);
    CS_PP_write(testBuffer1, TEST_WRITE_SIZE);
    CS_FAIL_ON_FALSE( CS_PP_dataSize( testBuffer1 ) == (TEST_READ_SIZE - TEST_WRITE_SIZE), "Partial write and read should end up with a known size.", "Sizes don't match." );
    CS_PP_reset(testBuffer1);
    
    char *temp = CS_tempBuff(256);
    for( int i = 0; i < TEST_BUFF_SIZE; ++i ) {
        temp[i] = (char)i;
    }

    int bytesWritten = 0;
    while( (bytesWritten += CS_PP_readFromBuffer( testBuffer1, temp + bytesWritten, TEST_WRITE_SIZE )) < TEST_BUFF_SIZE );
    for( int i = 0; i < TEST_BUFF_SIZE; ++i ) {
        CS_FAIL_ON_FALSE( temp[i] == testBuffer1->buff[i], "Known buffer contents written. (Ascending bytes)", "Buffer wrong at index %d", i);
    }

    temp = CS_tempBuff(256);
    for( int i = 0; i < TEST_BUFF_SIZE; ++i ) {
        temp[i] = (char)TEST_BUFF_SIZE - i - 1;
    }

    CS_PP_reset(testBuffer1);

    bytesWritten = 0;
    while( (bytesWritten += CS_PP_readFromBuffer( testBuffer1, temp + bytesWritten, TEST_WRITE_SIZE )) < TEST_BUFF_SIZE );
    for( int i = 0; i < TEST_BUFF_SIZE; ++i ) {
        CS_FAIL_ON_FALSE( temp[i] == testBuffer1->buff[i], "Known buffer contents written. (Descending bytes)", "Buffer wrong at index %d", i);
    }

    CS_PP_defaultFree( testBuffer1 );

    return testCount !=
           testSucceeded;
}


