#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/pipe.h>

extern bool test_pipe(void);

static int testCount = 0;
static int testSucceeded = 0;

#define NUM_BUFFS 4
#define BUFF_SIZE_MIN 4096
#define BUFF_SIZE_MAX 8001
#define SMALL_BUFF_SIZE 1000
#define COMPRESS_BUFF_SIZE 4096
#define BIG_BUFF_SIZE 8192

long fileSize( const char *filename ) {
    int handle = open(filename, O_RDONLY);
    if( handle < 0 ) return -1;
    struct stat statBuff;
    if( fstat(handle, &statBuff) < 0 ) {
        close(handle);
        return -1;
    }
    close(handle);
    return statBuff.st_size;
}

bool test_pipe(void) {
    //Tests go here:
    char **tbuffs = CS_allocZero( NUM_BUFFS * sizeof(char *) );
    int32_t *tsizes = CS_allocZero( NUM_BUFFS * sizeof(int32_t) );

    CS_pipeInitPipes( 32 );

    CS_FAIL_ON_NULL( tbuffs, "Allocating buffer for pointers.", "Failed." );
    CS_FAIL_ON_NULL( tsizes, "Allocating buffer for pointers.", "Failed." );
    if( tbuffs == NULL || tsizes == NULL ) {
        if( tsizes ) CS_free( tsizes );
        if( tbuffs ) CS_free( tbuffs );
        return true;
    }

    int32_t i;

    for( i = 0; i < NUM_BUFFS; ++i ) {
        int32_t newBuffSize = BUFF_SIZE_MIN + CS_testRandMax(BUFF_SIZE_MAX - BUFF_SIZE_MIN );
        tbuffs[i] = CS_alloc( newBuffSize );
        tsizes[i] = newBuffSize;
        if( tbuffs[ i ] ) {
            for( int j = 0; j < newBuffSize; j++ ) {
                tbuffs[i][j] = CS_testRandMax(255);
            }
        }
    }

    struct CS_PipeFileData tempFileOut1 = { "/tmp/tf1", "wb", NULL };
    struct CS_PipeFileData tempFileIn1 = { "/tmp/tf1", "rb", NULL };
    struct CS_PipeFileData tempFileOut2 = { "/tmp/tf2", "wb", NULL };
    struct CS_PipeFileData tempFileIn2 = { "/tmp/tf2", "rb", NULL };


    for( i = 0; i < NUM_BUFFS; ++i ) {
        struct CS_Pipe *source1 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *source2 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *source3 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *source4 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *dest1 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *dest2 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *dest3 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *dest4 = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *fileOut1 = CS_pipeCreate( CS_PIPE_FILE_OUT, 0, &tempFileOut1 );
        struct CS_Pipe *fileOut2 = CS_pipeCreate( CS_PIPE_FILE_OUT, SMALL_BUFF_SIZE, &tempFileOut2 );
        struct CS_Pipe *compress1 = CS_pipeCreate( CS_PIPE_DEFLATE, COMPRESS_BUFF_SIZE, NULL );
        struct CS_Pipe *compress2 = CS_pipeCreate( CS_PIPE_GZIP, COMPRESS_BUFF_SIZE, NULL );
        struct CS_Pipe *inflate1 = CS_pipeCreate( CS_PIPE_INFLATE, COMPRESS_BUFF_SIZE, NULL );
        struct CS_Pipe *inflate2 = CS_pipeCreate( CS_PIPE_GUNZIP, COMPRESS_BUFF_SIZE, NULL );
        struct CS_PushPullBuffer *sourceBits1 = CS_PP_onStaticBuffer( tsizes[i], tbuffs[i] );
        struct CS_PushPullBuffer *sourceBits2 = CS_PP_onStaticBuffer( tsizes[i], tbuffs[i] );
        struct CS_PushPullBuffer *sourceBits3 = CS_PP_onStaticBuffer( tsizes[i], tbuffs[i] );
        struct CS_PushPullBuffer *sourceBits4 = CS_PP_onStaticBuffer( tsizes[i], tbuffs[i] );
        struct CS_PushPullBuffer *destBits1 = CS_PP_defaultAlloc( BIG_BUFF_SIZE );
        struct CS_PushPullBuffer *destBits2 = CS_PP_defaultAlloc( BIG_BUFF_SIZE );
        struct CS_PushPullBuffer *destBits3 = CS_PP_defaultAlloc( BIG_BUFF_SIZE );
        struct CS_PushPullBuffer *destBits4 = CS_PP_defaultAlloc( BIG_BUFF_SIZE );
        CS_FAIL_ON_NULL( sourceBits1, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( sourceBits2, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( sourceBits3, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( sourceBits4, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( destBits1, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( destBits2, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( destBits3, "Push pull buffer available", "Nope" );
        CS_FAIL_ON_NULL( destBits4, "Push pull buffer available", "Nope" );
        if( sourceBits1 ) {
            source1->buffer = sourceBits1;
        } else {
            continue;
        }
        if( sourceBits2 ) {
            source2->buffer = sourceBits2;
        } else {
            continue;
        }
        if( sourceBits3 ) {
            source3->buffer = sourceBits3;
        } else {
            continue;
        }
        if( sourceBits4 ) {
            source4->buffer = sourceBits4;
        } else {
            continue;
        }
        if( destBits1 ) {
            dest1->buffer = destBits1;
        } else {
            continue;
        }
        if( destBits2 ) {
            dest2->buffer = destBits2;
        } else {
            continue;
        }
        if( destBits3 ) {
            dest3->buffer = destBits3;
        } else {
            continue;
        }
        if( destBits4 ) {
            dest4->buffer = destBits4;
        } else {
            continue;
        }
        CS_FAIL_ON_TRUE(CS_pipeHook( source1, fileOut1 ), "Pipes should hook.", "Pipe hook returned error.");
        CS_FAIL_ON_TRUE(CS_pipeHook( source2, fileOut2 ), "Pipes should hook.", "Pipe hook returned error.");
        CS_FAIL_ON_FALSE(CS_pipeProcess( fileOut1 ) == tsizes[i], "Wrote a file of the correct size.", "Not the correct sizes." );
        int32_t currentVal = 0;
        int32_t accumulatedSize = 0;
        while( true ) {
            currentVal = CS_pipeProcess( fileOut2 );
            CS_FAIL_ON_TRUE( currentVal < 0, "Processing the 2nd output pipe", "Returned -1" );
            if( currentVal > 0 ) accumulatedSize += currentVal;
            if( currentVal <= 0 ) {
                if( currentVal == 0 ) {
                    CS_FAIL_ON_FALSE( accumulatedSize == tsizes[i], "Accumulated size should be same as output size.", "Got %d instead of %d", accumulatedSize, tsizes[i] );
                }
                break;
            }
        }
        CS_PP_defaultFree(sourceBits1);
        CS_pipeClose(fileOut1);
        CS_pipeFree(fileOut1);
        CS_PP_defaultFree(sourceBits2);
        CS_pipeClose(fileOut2);
        CS_pipeFree(fileOut2);

        struct CS_Pipe *pipeIn1 = CS_pipeCreate( CS_PIPE_FILE_IN, BIG_BUFF_SIZE, &tempFileIn1 );
        CS_FAIL_ON_NULL( pipeIn1, "Failed to create input pipe.", "Pipe create fail." );
        struct CS_Pipe *pipeIn2 = CS_pipeCreate( CS_PIPE_FILE_IN, SMALL_BUFF_SIZE, &tempFileIn2 );
        CS_FAIL_ON_NULL( pipeIn2, "Failed to create input pipe.", "Pipe create fail." );

        CS_FAIL_ON_TRUE( CS_pipeHook( pipeIn1, dest1 ), "Hook pipe pieces.", "Fail" );
        CS_FAIL_ON_TRUE( CS_pipeHook( pipeIn2, dest2 ), "Hook pipe pieces.", "Fail" );
        CS_FAIL_ON_FALSE(CS_pipeProcess( pipeIn1 ) == tsizes[i], "Read a file of the correct size.", "Not the correct sizes." );
        currentVal = 0;
        accumulatedSize = 0;
        while( true ) {
            currentVal = CS_pipeProcess( pipeIn2 );
            CS_FAIL_ON_TRUE( currentVal < 0, "Processing the 2nd output pipe", "Returned -1" );
            if( currentVal > 0 ) accumulatedSize += currentVal;
            if( currentVal <= 0 ) {
                if( currentVal == 0 ) {
                    CS_FAIL_ON_FALSE( accumulatedSize == tsizes[i], "Accumulated size should be same as input size.", "Got %d instead of %d", accumulatedSize, tsizes[i] );
                }
                break;
            }
        }
        CS_FAIL_ON_TRUE( CS_pipeHook( source3, compress1 ), "Should hook.", "Fail!" );
        CS_FAIL_ON_TRUE( CS_pipeHook( compress1, inflate1 ), "Should hook.", "Fail!" );
        CS_FAIL_ON_TRUE( CS_pipeHook( inflate1, dest3 ), "Should hook.", "Fail!" );
        CS_FAIL_ON_TRUE( CS_pipeHook( source4, compress2 ), "Should hook.", "Fail!" );
        CS_FAIL_ON_TRUE( CS_pipeHook( compress2, inflate2 ), "Should hook.", "Fail!" );
        CS_FAIL_ON_TRUE( CS_pipeHook( inflate2, dest4 ), "Should hook.", "Fail!" );
        currentVal = 0;
        accumulatedSize = 0;
        while( true ) {
            currentVal = CS_pipeProcess( dest3 );
            if( currentVal > 0 ) accumulatedSize += currentVal;
            if( CS_pipeDoneOrError( dest3 ) ) {
                CS_FAIL_ON_FALSE( accumulatedSize == tsizes[i], "Accumulated size should be same as input size.", "Got %d instead of %d", accumulatedSize, tsizes[i] );
                break;
            }
        }
        currentVal = 0;
        accumulatedSize = 0;
        while( true ) {
            currentVal = CS_pipeProcess( dest4 );
            if( currentVal > 0 ) accumulatedSize += currentVal;
            if( CS_pipeDoneOrError( dest4 ) ) {
                CS_FAIL_ON_FALSE( accumulatedSize == tsizes[i], "Accumulated size should be same as input size.", "Got %d instead of %d", accumulatedSize, tsizes[i] );
                break;
            }
        }
        
        CS_FAIL_ON_FALSE( memcmp( tbuffs[i], CS_PP_startOfData( destBits1 ), tsizes[i] ) == 0, "Bytes read in should match original buffer.", "Did not!" );
        CS_FAIL_ON_FALSE( memcmp( tbuffs[i], CS_PP_startOfData( destBits2 ), tsizes[i] ) == 0, "Bytes read in should match original buffer.", "Did not!" );
        CS_FAIL_ON_FALSE( memcmp( tbuffs[i], CS_PP_startOfData( destBits3 ), tsizes[i] ) == 0, "Bytes read in should match original buffer.", "Did not!" );
        CS_FAIL_ON_FALSE( memcmp( tbuffs[i], CS_PP_startOfData( destBits4 ), tsizes[i] ) == 0, "Bytes read in should match original buffer.", "Did not!" );
        CS_pipeClose( dest1 );
        CS_pipeClose( dest2 );
        CS_pipeClose( dest3 );
        CS_pipeClose( dest4 );
        CS_pipeFree( dest1 );
        CS_pipeFree( dest2 );
        CS_pipeFree( dest3 );
        CS_pipeFree( dest4 );
        CS_PP_defaultFree( destBits1 );
        CS_PP_defaultFree( destBits2 );
        CS_PP_defaultFree( destBits3 );
        CS_PP_defaultFree( destBits4 );
    }

    CS_pipeDestroyPipes();

    return testCount !=
           testSucceeded;
}


