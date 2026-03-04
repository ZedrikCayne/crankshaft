#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/tempbuff.h>

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

    CS_PP_reset(testBuffer1);
    CS_PP_readFromBuffer( testBuffer1, temp, TEST_BUFF_SIZE );
    CS_PP_write( testBuffer1, 15 );
    CS_PP_makeRoom( testBuffer1 );
    CS_FAIL_ON_FALSE( CS_PP_bufferRemaining( testBuffer1 ) == 15, "Buffer should have 15 bytes left after writing 15 and then making room.", "%d left", CS_PP_bufferRemaining( testBuffer1 ) );

    CS_PP_reset(testBuffer1);
    CS_PP_readFromBuffer( testBuffer1, temp, TEST_BUFF_SIZE );
    CS_PP_removeChunk( testBuffer1, 0, 15 );
    for( int i = 0; i < TEST_BUFF_SIZE - 15; ++i ) {
        CS_FAIL_ON_FALSE( temp[i + 15] == testBuffer1->buff[i], "Known buffer contents written. (Descending bytes with 15 bytes removed off the front)", "Buffer wrong at index %d", i );
    }

    CS_PP_reset(testBuffer1);
    CS_PP_readFromBuffer( testBuffer1, temp, TEST_BUFF_SIZE);
    CS_PP_removeChunk( testBuffer1, 32, 15 );
    for( int i = 0; i < 32; ++i ) {
        CS_FAIL_ON_FALSE( temp[i] == testBuffer1->buff[i], "Known buffer contents written. (Descending bytes with 15 bytes removed at offset 32)", "Buffer wrong at index %d", i );
    }
    for( int i = 32; i < TEST_BUFF_SIZE - 15; ++i ) {
        CS_FAIL_ON_FALSE( temp[i + 15] == testBuffer1->buff[i], "Known buffer contents written. (Descending bytes with 15 bytes removed at offset 32)", "Buffer wrong at index %d", i );
    }

    CS_PP_reset(testBuffer1);
    CS_PP_readFromBuffer( testBuffer1, temp, TEST_BUFF_SIZE);
    CS_FAIL_ON_TRUE( CS_PP_removeChunk( testBuffer1, TEST_BUFF_SIZE - 15, 15 ), "Removing the last 15 bytes should work.", "This errored." );
    CS_FAIL_ON_FALSE( CS_PP_removeChunk( testBuffer1, TEST_BUFF_SIZE - 15, 15 ), "Removing the last 15 bytes should not work if beyond the end of the written buffer.", "This worked, not cool." );

    struct CS_PushPullBuffer *testBuffer2 = CS_PP_defaultAlloc( TEST_BUFF_SIZE / 2 );
    CS_PP_reset(testBuffer1);
    CS_PP_readFromBuffer( testBuffer1, temp, TEST_BUFF_SIZE);
    CS_FAIL_ON_FALSE( CS_PP_moveBuffer( testBuffer1, testBuffer2 ) == ( TEST_BUFF_SIZE / 2 ), "Copying from a large buffer to small buffer return the size of the smaller buffer.", "Did not copy the correct size bytes." );
    CS_FAIL_ON_FALSE( CS_PP_dataSize( testBuffer2 ) == (TEST_BUFF_SIZE / 2), "Moving bytes frone buffer to another should have the right reported number of bytes.", "Wrong number of bytes." );
    CS_FAIL_ON_FALSE( CS_PP_dataSize( testBuffer1 ) == (TEST_BUFF_SIZE / 2), "Moving bytes from one buffer to another should half half the size of the source.", "Source not empty." );
    CS_PP_reset( testBuffer2 );
    CS_FAIL_ON_FALSE( CS_PP_moveBuffer( testBuffer1, testBuffer2 ) == ( TEST_BUFF_SIZE / 2 ), "Copying from a large buffer that is half full to small buffer return the size of the smaller buffer.", "Did not copy the correct size bytes." );
    CS_FAIL_ON_FALSE( CS_PP_dataSize( testBuffer1 ) == 0, "Moving bytes from one buffer to another should half half the size of the source.", "Source not empty." );
    CS_FAIL_ON_FALSE( CS_PP_dataSize( testBuffer2 ) == (TEST_BUFF_SIZE / 2), "Moving bytes frone buffer to another should have the right reported number of bytes.", "Wrong number of bytes." );

    CS_PP_defaultFree( testBuffer1 );
    CS_PP_defaultFree( testBuffer2 );

    return testCount !=
           testSucceeded;
}


