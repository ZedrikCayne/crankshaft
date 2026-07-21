#include <stdlib.h>
#include <stdio.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/pipe.h>
#include <crankshaft/slaballoc.h>

#include <zlib.h>

static struct CS_SlabAllocator *globalPipes = NULL;

bool CS_pipeInitPipes( int32_t initialPipes ) {
    if( !globalPipes ) {
        globalPipes = CS_slabInit("Global Pipes", sizeof(struct CS_Pipe), initialPipes, sizeof(void*) );
    }
    return globalPipes == NULL;
}

bool CS_pipeDestroyPipes( void ) {
    if( globalPipes ) {
        CS_slabFree( globalPipes );
        globalPipes = NULL;
    }
    return false;
}


struct CS_Pipe *CS_pipeCreate( const struct CS_PipeDefinition *pipeType,
                               int32_t bufferSize,
                               void *pipeData ) {
    if( !globalPipes ) return NULL;
    if( pipeType->pipeFlags & CS_PIPE_FLAG_NO_BUFFER && bufferSize ) {
        CS_LOG_ERROR("Trying to create a pipe that will initialize its own buffer with a requested buffer size")
        return NULL;
    }
    struct CS_Pipe *returnValue = CS_slabTakeZero(globalPipes);
    if( returnValue ) {
        returnValue->flags = pipeType->pipeFlags;
        returnValue->pipeProcess = pipeType->pipeProcess;
        returnValue->pipeClose = pipeType->pipeClose;
        if( bufferSize > 0 ) {
            returnValue->buffer = CS_PP_defaultAlloc(bufferSize);
            if( returnValue->buffer == NULL ) {
                goto ERR_PIPE;
            }
            returnValue->flags |= CS_PIPE_FLAG_OWN_BUFFER;
        }
        if( pipeType->pipeCreate && pipeType->pipeCreate( returnValue, pipeData ) ) {
            goto ERR_INIT;
        }
    }
    return returnValue;
ERR_INIT:
    if( returnValue->buffer && (returnValue->flags&CS_PIPE_FLAG_OWN_BUFFER)) CS_PP_defaultFree(returnValue->buffer);
ERR_PIPE:
    if( returnValue ) CS_slabReturn( globalPipes, returnValue );
    return NULL;
}

struct CS_Pipe *privateFront( struct CS_Pipe *anyStage ) {
    if( anyStage == NULL ) return NULL;

    struct CS_Pipe *currentPipe = anyStage;

    while( currentPipe->in &&
           !( currentPipe->statusFlags &
               (CS_PIPE_STATUS_EMPTY|CS_PIPE_STATUS_ERROR|CS_PIPE_STATUS_CLOSED) ) ) {
        currentPipe = currentPipe->in;
    }

    //Stop if we're fragile and hit an error.
    if( currentPipe->flags & CS_PIPE_FLAG_FRAGILE &&
        currentPipe->statusFlags & CS_PIPE_STATUS_ERROR )
        return currentPipe;

    //If we're at a closed or empty portion move forward.
    while( currentPipe &&
           (currentPipe->statusFlags & (CS_PIPE_STATUS_EMPTY|CS_PIPE_STATUS_CLOSED|CS_PIPE_STATUS_ERROR))  ) {
        if( currentPipe->flags & CS_PIPE_FLAG_FRAGILE &&
            currentPipe->statusFlags & CS_PIPE_STATUS_ERROR ) {
            return currentPipe;
        }
        currentPipe = currentPipe->next;
    }

    return currentPipe;
}

//If we are set empty, or if we don't have a buffer and any previous
//step is empty, we are empty.
bool CS_pipeEmpty( struct CS_Pipe *thisStage ) {
    struct CS_Pipe *currentStage = thisStage;
    while(true) {
        if( currentStage == NULL ) return true;
        if( currentStage->buffer && (currentStage->statusFlags & CS_PIPE_STATUS_EMPTY) ) return true;
        if( currentStage->buffer == NULL ) {
            currentStage = currentStage->in;
        } else {
            return false;
        }
    }
}


//Wander back and find the nearest 'in' buffer. (Stages might not have their
//own buffer and just rely on a previous buffer... 
struct CS_PushPullBuffer *CS_pipeNearestInBuffer( const struct CS_Pipe *currentSection ) {
    if( currentSection == NULL ) return NULL;
    while( currentSection->in && 
          (currentSection->in->buffer == NULL) )
        currentSection = currentSection->in;
    if( currentSection->in == NULL ) return NULL;
    return currentSection->in->buffer;
}


static struct CS_PushPullBuffer *copyFromNearest( struct CS_Pipe *currentSection ) {
    struct CS_PushPullBuffer *nearest = CS_pipeNearestInBuffer( currentSection );
    struct CS_PushPullBuffer *returnValue = nearest;
    if( currentSection->buffer ) {
        returnValue = currentSection->buffer;
        if( nearest ) CS_PP_moveBuffer( nearest, returnValue );
    }
    return returnValue;
}

int32_t CS_pipeProcess( struct CS_Pipe *anyStage ) {
    struct CS_Pipe *currentPipe = privateFront(anyStage);
    int32_t returnValue = 0;
    //Get to the 'front' most bit of the pipe.
    while( currentPipe ) {
        if( currentPipe->pipeProcess ) {
            returnValue = currentPipe->pipeProcess( currentPipe );
        } else {
            copyFromNearest( currentPipe );
        }
        if( returnValue < 0 ) {
            currentPipe->statusFlags |= CS_PIPE_STATUS_ERROR;
            if( currentPipe->flags | CS_PIPE_FLAG_FRAGILE ) return -1;
        }
        //If our buffer is empty, and the next previous step is empty, we
        //must be empty.
        if( currentPipe->buffer && CS_PP_dataSize( currentPipe->buffer ) == 0 &&
            CS_pipeEmpty( currentPipe->in ) ) {
            currentPipe->statusFlags |= CS_PIPE_STATUS_EMPTY;
        }
        //If we are the end of the pipe...and our previous state is empty
        //And we don't process anything (ie: we're holding a buffer) we are 'EMPTY'
        //if our previous stuff is empty so loops will end.
        if( currentPipe->pipeProcess == NULL && currentPipe->next == NULL && CS_pipeEmpty( currentPipe->in ) ) {
            currentPipe->statusFlags |= CS_PIPE_STATUS_EMPTY;
        }
        currentPipe = currentPipe->next;
    }
    return returnValue;
}

int32_t CS_pipeStatus( struct CS_Pipe *anyStage ) {
    struct CS_Pipe *currentPipe = anyStage;
    if( currentPipe == NULL ) return CS_PIPE_STATUS_ERROR;
    int32_t returnValue = 0;
    while( currentPipe->in ) currentPipe = currentPipe->in;
    while( currentPipe ) {
        returnValue |= currentPipe->statusFlags;
        currentPipe = currentPipe->next;
    }
    return returnValue;
}

int32_t CS_pipeClose( struct CS_Pipe *anyStage ) {
    struct CS_Pipe *currentPipe = anyStage;
    while( currentPipe->in ) currentPipe = currentPipe->in;
    while( currentPipe ) {
        if( currentPipe->pipeClose &&
            !(currentPipe->statusFlags & CS_PIPE_STATUS_CLOSED) ) {
            currentPipe->pipeClose( currentPipe );
        }
        currentPipe->statusFlags |= CS_PIPE_STATUS_CLOSED;
        currentPipe = currentPipe->next;
    }
    return 0;
}

int32_t CS_pipeFree( struct CS_Pipe *anyStage ) {
    CS_pipeClose(anyStage);
    struct CS_Pipe *currentPipe = anyStage;
    struct CS_Pipe *lastPipe = NULL;
    while( currentPipe && currentPipe->in ) currentPipe = currentPipe->in;
    while( currentPipe ) {
        lastPipe = NULL;
        if( !(currentPipe->statusFlags & CS_PIPE_STATUS_FREE ) ) {
            if( currentPipe->buffer && (currentPipe->flags&CS_PIPE_FLAG_OWN_BUFFER)) CS_PP_defaultFree(currentPipe->buffer);
            currentPipe->statusFlags |= CS_PIPE_STATUS_FREE;
            lastPipe = currentPipe;
        }
        currentPipe = currentPipe->next;
        if( lastPipe ) CS_slabReturn( globalPipes, lastPipe );
    }
    return 0;
}

bool CS_pipeHook( struct CS_Pipe *in, struct CS_Pipe *out ) {
    if( in->next ) return true;
    if( out->in ) return true;
    in->next = out; out->in = in;
    return 0;
}

struct CS_Pipe *CS_pipeReconnect( struct CS_Pipe *in, struct CS_Pipe *out) {
    struct CS_Pipe *returnValue = NULL;
    if( in->next && out->in ) returnValue = CS_PIPE_BAD_RECONNECT;
    if( in->next ) returnValue = in->next;
    if( out->in ) returnValue = out->in;
    in->next = out; out->in = in;
    return returnValue;
}

bool CS_pipeDoneOrError( struct CS_Pipe *anyStage ) {
    struct CS_Pipe *currentPipe = anyStage;
    while( currentPipe->next ) currentPipe = currentPipe->next;
    if( currentPipe->statusFlags & (CS_PIPE_STATUS_EMPTY|CS_PIPE_STATUS_ERROR) ) return true;
    while( currentPipe->in ) {
        currentPipe = currentPipe->in;
        if( currentPipe->statusFlags & CS_PIPE_STATUS_ERROR ) return true;
        if( currentPipe->statusFlags & CS_PIPE_STATUS_EMPTY ) return false;
    }
    return false;
}

//DEFAULT PIPE PIECES!
/*
struct CS_PipeDefinition {
    const int32_t pipeFlags;
    int32_t (*pipeProcess)( struct CS_Pipe *stage );
    bool (*pipeClose)( struct CS_Pipe *stage );
    bool (*pipeCreate)( struct CS_Pipe *stage, const void *initialPipeData );
};
*/

//The implementation of the pushpull will return 0 every time you try to read an EOF.
static int32_t _file_in( struct CS_Pipe *currentSection ) {
    int32_t returnValue = CS_PP_readFromFILE( currentSection->buffer, (FILE*)currentSection->pipeData );
    if( returnValue < 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
    if( returnValue == 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_EOF;
    return returnValue;
}

static int32_t _file_out( struct CS_Pipe *currentSection ) {
    struct CS_PushPullBuffer *currentBuffer = copyFromNearest( currentSection );

    int32_t returnValue = CS_PP_writeToFILE( currentBuffer, (FILE*)currentSection->pipeData );
    if( returnValue < 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
    return returnValue;
}

static bool _file_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    const struct CS_PipeFileData *initialFileData = (const struct CS_PipeFileData *)initialPipeData;
    if( initialFileData->fileName ) {
        FILE *openFile = fopen(initialFileData->fileName, initialFileData->mode);
        if( openFile == NULL ) {
            currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
            return true;
        }
        currentSection->pipeData = openFile;
        currentSection->flags |= CS_PIPE_FLAG_OWN_DATA;
    } else {
        currentSection->pipeData = initialFileData->filePointer;
    }
    return false;
}

static bool _file_close( struct CS_Pipe *currentSection ) {
    if( currentSection->pipeData && (currentSection->flags & CS_PIPE_FLAG_OWN_DATA) ) {
        fflush( (FILE*)currentSection->pipeData );
        fclose( (FILE*)currentSection->pipeData );
    }
    return false;
}

static const struct CS_PipeDefinition _CS_PIPE_FILE_IN = {
    0,_file_in,_file_close,_file_create
};
const struct CS_PipeDefinition *CS_PIPE_FILE_IN = &_CS_PIPE_FILE_IN;
static const struct CS_PipeDefinition _CS_PIPE_FILE_OUT = {
    0,_file_out,_file_close,_file_create
};
const struct CS_PipeDefinition *CS_PIPE_FILE_OUT = &_CS_PIPE_FILE_OUT;

static int32_t _socket_in( struct CS_Pipe *currentSection ) {
    int32_t returnValue = CS_PP_readFromFile( currentSection->buffer, (int32_t)(uintptr_t)currentSection->pipeData );
    if( returnValue < 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
    return returnValue;
}

static int32_t _socket_out( struct CS_Pipe *currentSection ) {
    struct CS_PushPullBuffer *currentBuffer = copyFromNearest( currentSection );

    int32_t returnValue = CS_PP_writeToFile( currentBuffer, (int32_t)(uintptr_t)currentSection->pipeData );
    if( returnValue < 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
    return returnValue;
}

static bool _socket_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    currentSection->pipeData = (void*)(uintptr_t)((const struct CS_PipeSocketData *)initialPipeData)->socketHandle;
    return false;
}

static bool _socket_close( struct CS_Pipe *currentSection ) {
    return false;
}

static const struct CS_PipeDefinition _CS_PIPE_SOCKET_IN = {
    0, _socket_in, _socket_close, _socket_create
};
const struct CS_PipeDefinition *CS_PIPE_SOCKET_IN = &_CS_PIPE_SOCKET_IN;
static const struct CS_PipeDefinition _CS_PIPE_SOCKET_OUT = {
    0, _socket_out, _socket_close, _socket_create
};
const struct CS_PipeDefinition *CS_PIPE_SOCKET_OUT = &_CS_PIPE_SOCKET_OUT;

static int32_t _ssl_in( struct CS_Pipe *currentSection ) {
    int32_t returnValue = CS_PP_readFromSSL( currentSection->buffer, (SSL*)currentSection->pipeData );
    if( returnValue < 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
    return returnValue;
}

static int32_t _ssl_out( struct CS_Pipe *currentSection ) {
    struct CS_PushPullBuffer *currentBuffer = copyFromNearest( currentSection );

    int32_t returnValue = CS_PP_writeToSSL( currentBuffer, (SSL*)currentSection->pipeData );
    if( returnValue < 0 ) currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
    return returnValue;
}

static bool _ssl_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    currentSection->pipeData = ((const struct CS_PipeSSLData *)initialPipeData)->ssl;
    return false;
}

static bool _ssl_close( struct CS_Pipe *currentSection ) {
    return false;
}

static const struct CS_PipeDefinition _CS_PIPE_SSL_IN = {
    0, _ssl_in, _ssl_close, _ssl_create
};
const struct CS_PipeDefinition *CS_PIPE_SSL_IN = &_CS_PIPE_SSL_IN;
static const struct CS_PipeDefinition _CS_PIPE_SSL_OUT = {
    0, _ssl_out, _ssl_close, _ssl_create
};
const struct CS_PipeDefinition *CS_PIPE_SSL_OUT = &_CS_PIPE_SSL_OUT;

static const struct CS_PipeDefinition _CS_PIPE_NULL = {
    0, NULL, NULL, NULL
};
const struct CS_PipeDefinition *CS_PIPE_NULL = &_CS_PIPE_NULL;

struct compress_pipe_state {
    z_stream *stream;
    int32_t bytesAlreadyProcessed;
    bool weDone;
};

// Custom allocators for zlib
static voidpf cs_zalloc(voidpf opaque, uInt items, uInt size) {
    return (voidpf)CS_alloc(items * size);
}

static void cs_zfree(voidpf opaque, voidpf address) {
    CS_free(address);
}

//Compress pipe pieces.
static int32_t driveZlib( struct CS_Pipe *currentSection, bool compress ) {
    struct compress_pipe_state *state = (struct compress_pipe_state *)currentSection->pipeData;
    if( !state ) return -1;
    struct CS_PushPullBuffer *inBuff = CS_pipeNearestInBuffer( currentSection );
    if( !inBuff ) return -1;

    //If we can't push out... continue down the pipe so hopefully someone will.
    //empty our buffer.
    if( CS_PP_bufferRemaining( currentSection->buffer ) == 0 ) return 0;

    state->stream->next_in = (Bytef *)CS_PP_startOfData(inBuff);
    state->stream->avail_in = CS_PP_dataSize(inBuff);
    state->stream->next_out = (Bytef *)CS_PP_endOfData(currentSection->buffer);
    state->stream->avail_out = CS_PP_bufferRemaining(currentSection->buffer);
    long out_before = state->stream->total_out;
    long in_before = state->stream->total_in;

    bool inEmpty = CS_pipeEmpty(currentSection->in);

    int32_t flushFlag = inEmpty?Z_FINISH:Z_NO_FLUSH;
    int32_t zStatus = 0;
    if( compress ) {
        zStatus = deflate( state->stream, flushFlag );
    } else {
        zStatus = inflate( state->stream, flushFlag );
    }
    
    if( zStatus < 0 ) {
        currentSection->statusFlags |= CS_PIPE_STATUS_ERROR;
        return -1;
    }
    long bytesWritten = state->stream->total_out - out_before;
    long bytesRead = state->stream->total_in - in_before;
    //Move the push pull buffers...by 'writing' from the input buffer and
    //'reading' into the output buffer.
    CS_PP_write(inBuff, bytesRead);
    CS_PP_read(currentSection->buffer, bytesWritten);
    return bytesWritten;
}

static int32_t _inflate( struct CS_Pipe *currentSection ) {
    return driveZlib( currentSection, false );
}

static int32_t _deflate( struct CS_Pipe *currentSection ) {
    return driveZlib(currentSection, true);
}

static bool createCompression( struct CS_Pipe *currentSection, bool compress, bool gzip ) {
    currentSection->pipeData = CS_allocZero(sizeof(struct compress_pipe_state));
    if( currentSection->pipeData ) {
        struct compress_pipe_state *state = currentSection->pipeData;
        state->stream = CS_allocZero(sizeof(z_stream));
        if( state->stream ) {
            state->stream->zalloc = cs_zalloc;
            state->stream->zfree = cs_zfree;
            state->stream->opaque = NULL;
            int32_t ret;
            if( compress ) {
                if( gzip ) {
                    ret = deflateInit2(state->stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
                } else {
                    ret = deflateInit(state->stream, Z_DEFAULT_COMPRESSION);
                }
            } else {
                if( gzip ) {
                    ret = inflateInit2(state->stream, 47);
                } else {
                    ret = inflateInit(state->stream);
                }
            }
            if( ret != Z_OK ) {
                CS_free(state->stream);
                CS_free(currentSection->pipeData);
                currentSection->pipeData = NULL;
            }
        } else {
            CS_free(currentSection->pipeData);
            currentSection->pipeData = NULL;
        }
    }
    return currentSection->pipeData == NULL;
}

static bool destroyCompression( struct CS_Pipe *currentSection, bool compress, bool gzip ) {
    if( currentSection->pipeData ) {
        struct compress_pipe_state *state = currentSection->pipeData;
        if( state->stream ) {
            if( compress ) {
                deflateEnd(state->stream);
            } else {
                inflateEnd(state->stream);
            }
            CS_free( state->stream );
            state->stream = NULL;
        }
        CS_free( currentSection->pipeData );
        currentSection->pipeData = NULL;
    }
    return false;
}

static bool _compress_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    return createCompression(currentSection, true, false);
}
static bool _compress_close( struct CS_Pipe *currentSection ) {
    return destroyCompression(currentSection, true, false);
}
static bool _inflate_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    return createCompression(currentSection, false, false);
}
static bool _inflate_close( struct CS_Pipe *currentSection ) {
    return destroyCompression(currentSection, false, false);
}
static bool _gzip_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    return createCompression(currentSection, true, true);
}
static bool _gzip_close( struct CS_Pipe *currentSection ) {
    return destroyCompression(currentSection, true, true);
}
static bool _gunzip_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    return createCompression(currentSection, false, true);
}
static bool _gunzip_close( struct CS_Pipe *currentSection ) {
    return destroyCompression(currentSection, false, true);
}

static const struct CS_PipeDefinition _CS_PIPE_INFLATE = {
    0, _inflate, _inflate_close, _inflate_create
};
const struct CS_PipeDefinition *CS_PIPE_INFLATE = &_CS_PIPE_INFLATE;

static const struct CS_PipeDefinition _CS_PIPE_DEFLATE = {
    0, _deflate, _compress_close, _compress_create
};

static int32_t _gzip( struct CS_Pipe *currentSection ) {
    return driveZlib(currentSection,true);
}

const struct CS_PipeDefinition *CS_PIPE_DEFLATE = &_CS_PIPE_DEFLATE;
static const struct CS_PipeDefinition _CS_PIPE_GZIP = {
    0, _gzip, _gzip_close, _gzip_create
};

static int32_t _gunzip( struct CS_Pipe *currentSection ) {
    return driveZlib(currentSection, false);
}

const struct CS_PipeDefinition *CS_PIPE_GZIP = &_CS_PIPE_GZIP;
static const struct CS_PipeDefinition _CS_PIPE_GUNZIP = {
    0, _gunzip, _gunzip_close, _gunzip_create
};
const struct CS_PipeDefinition *CS_PIPE_GUNZIP = &_CS_PIPE_GUNZIP;

struct limited_pipe_data {
    bool movedAll;
    int32_t currentBytesTransferred;
    int32_t maxBytesTransferred;
};
static int32_t _limited( struct CS_Pipe *currentSection ) {
    struct limited_pipe_data *pipeData = (struct limited_pipe_data *)currentSection->pipeData;
    struct CS_PushPullBuffer *inBuff = CS_pipeNearestInBuffer( currentSection );
    if( !inBuff ) return -1;
    int32_t bytesMoved = 0;
    if( CS_PP_dataSize(inBuff) > 0 && !pipeData->movedAll ) {
        int32_t sizeIn = CS_PP_dataSize(inBuff);
        int32_t roomOut = CS_PP_bufferRemaining(currentSection->buffer);
        int32_t maxBytes = pipeData->maxBytesTransferred - pipeData->currentBytesTransferred;
        int32_t toMove = sizeIn;
        if( roomOut < toMove ) toMove = roomOut;
        if( maxBytes < toMove ) toMove = maxBytes;
        bytesMoved = CS_PP_moveBufferExplicit( inBuff, currentSection->buffer, toMove );
        pipeData->currentBytesTransferred += bytesMoved;
        pipeData->movedAll = pipeData->currentBytesTransferred >= pipeData->maxBytesTransferred;
    }
    //Set ourself empty. We shouldn't go back to the previous bit even after this.
    if( CS_PP_dataSize(currentSection->buffer) == 0 ) {
        if( pipeData->movedAll ) currentSection->statusFlags |= CS_PIPE_STATUS_EMPTY;
    }
    return bytesMoved;
}
static bool _limited_close( struct CS_Pipe *currentSection ) {
    struct limited_pipe_data *pipeData = (struct limited_pipe_data *)currentSection->pipeData;
    if( pipeData ) CS_free(pipeData);
    return pipeData == NULL;
}
static bool _limited_create( struct CS_Pipe *currentSection, const void *initialPipeData ) {
    if( initialPipeData == NULL ) return true;
    int32_t wantedPipeSize = *(CS_PipeLimitedData*)initialPipeData;
    if( wantedPipeSize == 0 ) return true;
    currentSection->pipeData = CS_allocZero(sizeof(struct limited_pipe_data));
    if( currentSection->pipeData ) {
        struct limited_pipe_data *pipeData = (struct limited_pipe_data *)currentSection->pipeData;
        pipeData->maxBytesTransferred = wantedPipeSize;
    }
    return currentSection->pipeData == NULL;
}
static const struct CS_PipeDefinition _CS_PIPE_LIMITED = {
    0, _limited, _limited_close, _limited_create
};
const struct CS_PipeDefinition *CS_PIPE_LIMITED = &_CS_PIPE_LIMITED;
