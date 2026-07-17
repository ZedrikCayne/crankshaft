#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>
#include <crankshaft/pushpull.h>

#include <crankshaft/pipe.h>

#include <crankshaft/http.h>

extern bool test_chunk(void);

static int testCount = 0;
static int testSucceeded = 0;

static char encoded0[] = "8\r\nWelcome \r\n1c\r\nto Mozilla Developer Network\r\n0\r\n";
static char decoded0[] = "Welcome to Mozilla Developer";

#define NUM_BUFFS 4
#define BUFF_SIZE_MIN 4096
#define BUFF_SIZE_MAX 8001
#define SMALL_BUFF_SIZE 1000
#define COMPRESS_BUFF_SIZE 4096
#define BIG_BUFF_SIZE 8192

bool test_chunk(void) {
    //Tests go here:
    struct CS_PushPullBuffer *pp = CS_PP_onStaticBuffer( sizeof(encoded0), encoded0 );

    CS_pipeInitPipes( 32 );

    CS_FAIL_ON_NULL(pp, "Push pull buffer for first encode.", "Fail");

    struct CS_Pipe *input = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
    CS_FAIL_ON_NULL(input, "Null input pipe create.", "Fail.");
    if( input ) {
        input->buffer = pp;
        struct CS_Pipe *decode = CS_pipeCreate( CS_PIPE_CHUNK_DECODE, 4096, NULL );
        CS_FAIL_ON_NULL( decode, "Null output pipe.", "Faile" );
        if( decode ) {
            CS_FAIL_ON_TRUE(CS_pipeHook(input, decode), "Hook input to decode", "Fail");
            struct CS_Pipe *output = CS_pipeCreate( CS_PIPE_NULL, 4096, NULL );
            CS_FAIL_ON_NULL( output, "Null output pipe.", "Faile" );
            if( output ) {
                CS_FAIL_ON_TRUE(CS_pipeHook(decode, output), "Hook decode to output", "Fail");
                while( !CS_pipeDoneOrError( output ) ) CS_pipeProcess(output);
                CS_FAIL_ON_FALSE( memcmp(decoded0, CS_PP_startOfData(output->buffer), strlen(decoded0) ) == 0, "Check encoded vs decodes.", "Failed." );
            }
        }
        CS_pipeFree( input );
    }

    CS_PP_defaultFree(pp);

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

    char *outBuff = CS_alloc(BIG_BUFF_SIZE);

    for( i = 0; i < NUM_BUFFS; ++i ) {
        struct CS_PushPullBuffer ppIn = {0};
        struct CS_PushPullBuffer ppOut = {0};
        memset(outBuff, 0, BIG_BUFF_SIZE);
        CS_PP_init(&ppIn, tsizes[i], tbuffs[i]);
        CS_PP_init(&ppOut, BIG_BUFF_SIZE, outBuff );
        CS_PP_read(&ppIn, tsizes[i]);
        struct CS_Pipe *in = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        struct CS_Pipe *out = CS_pipeCreate( CS_PIPE_NULL, 0, NULL );
        in->buffer = &ppIn;
        out->buffer = &ppOut;
        int32_t wantedBuffSize = 440;
        struct CS_Pipe *encode = CS_pipeCreate(CS_PIPE_CHUNK_ENCODE, SMALL_BUFF_SIZE, &wantedBuffSize );
        struct CS_Pipe *decode = CS_pipeCreate(CS_PIPE_CHUNK_DECODE, SMALL_BUFF_SIZE, NULL );
        CS_pipeHook(in, encode);
        CS_pipeHook(encode, decode);
        CS_pipeHook(decode, out);
        while( !CS_pipeDoneOrError( decode ) ) CS_pipeProcess(decode);
        CS_FAIL_ON_FALSE( CS_PP_dataSize(&ppOut) == tsizes[i], "Check sizes.", "%d:%d", CS_PP_dataSize(&ppOut), tsizes[i] );
        CS_FAIL_ON_FALSE( memcmp( tbuffs[i],outBuff, tsizes[i] ) == 0, "Check contents", "Failed" );
        CS_pipeClose(out);
        CS_pipeFree(out);
    }

    CS_free(outBuff);
    for( i = 0; i < NUM_BUFFS; ++i ) {
        CS_free(tbuffs[i]);
    }
    CS_free(tbuffs);
    CS_free(tsizes);

    CS_pipeDestroyPipes();

    return testCount !=
           testSucceeded;
}


