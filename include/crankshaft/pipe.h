#ifndef __crankshaftpipedoth__
#define __crankshaftpipedoth__
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/socket.h>
#include <openssl/ssl.h>

#include <crankshaft/pushpull.h>

/********************************************************************
 *
 * Pipe handling. When you create a new CS_Pipe you need to provide
 * it with two functions. One to delete the current pipe's pipeData,
 * and one to process the input side.
 *
 * CS_pipeProcess will go to the first node in the pipe that is not
 * 'empty' and call the pipeProcess function. Then go onto the next
 * section of the pipe and continue till the end.
 *
 * Returning the last value of the pipe process function. Which is
 * the number of bytes processed into the current buffer.
 *
 * Status flags:
 *
 * Pipe has 'EMPTY':      This section of pipe has processed all
 *                        that it ever will.
 *
 * Pipe has 'ERROR':      This section of pipe has encountered an
 *                        unrecoverable error. By default pipes
 *                        should empty themselves fully but I'm not
 *                        your responsible adult.
 *
 * Pipe has 'WOULDBLOCK': If set the pipe would block if it had been
 *                        set to BLOCK.
 *
 * Property flags;
 *
 * Pipe has 'NOBLOCK':    If set, pipe should not block on trying to
 *                        fill. For example, if this is a pipe
 *                        incoming from a socket and there's nothing
 *                        there, it should wait. Note: this will
 *                        block the entire pipe process.
 *                        Note 2: This flag is very pipe specific.
 *                        Some pipe types should not block. (For
 *                        example, a buffer decompress pipe)
 *
 * Pipe has 'FRAGILE':    Fragile pipes should close on any
 *                        errors. Library provided pipe pieces
 *                        will honor this, but you're free to do
 *                        what you like. By default pipes will try
 *                        to process all data still left inside.
 *
 * Pipe has 'OWN_BUFFER': The pipe 'owns' the buffer and should free
 *                        it via the default means when done.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define CS_PIPE_STATUS_EMPTY        0x00000001
#define CS_PIPE_STATUS_ERROR        0x00000002
#define CS_PIPE_STATUS_WOULDBLOCK   0x00000004
#define CS_PIPE_STATUS_CLOSED       0x00000008
#define CS_PIPE_STATUS_EOF          0x00000010

#define CS_PIPE_FLAG_FRAGILE        0x00000001
#define CS_PIPE_FLAG_NOBLOCK        0x00000002
#define CS_PIPE_FLAG_OWN_BUFFER     0x00000004
#define CS_PIPE_FLAG_OWN_DATA       0x00000008

#define CS_PIPE_BAD_RECONNECT       ((struct CS_Pipe *)-1)

struct CS_Pipe {
    int32_t flags;
    int32_t statusFlags;
    struct CS_Pipe *in;
    struct CS_Pipe *next;
    struct CS_PushPullBuffer *buffer;
    void *pipeData;
    int32_t (*pipeProcess)( struct CS_Pipe *stage );
    bool (*pipeClose)( struct CS_Pipe *stage );
};

struct CS_PipeDefinition {
    const int32_t pipeFlags;
    int32_t (*pipeProcess)( struct CS_Pipe *stage );
    bool (*pipeClose)( struct CS_Pipe *stage );
    bool (*pipeCreate)( struct CS_Pipe *stage, const void *initialPipeData );
};

//When opening a 'standard' file pipe... provide a file name or FILE pointer.
//(not both)
struct CS_PipeFileData {
    const char *fileName;
    const char *mode;
    FILE *filePointer;
};

//File pipe pieces, copies from FILE* to buffer or buffer to FILE*
extern const struct CS_PipeDefinition *CS_PIPE_FILE_IN;
extern const struct CS_PipeDefinition *CS_PIPE_FILE_OUT;

//Provide an already open socket
struct CS_PipeSocketData {
    int32_t socketHandle;
};

//Socket pipe pieces, copies from file handle to buffer or buffer to file handle
extern const struct CS_PipeDefinition *CS_PIPE_SOCKET_IN;
extern const struct CS_PipeDefinition *CS_PIPE_SOCKET_OUT;

//Provide a ssl to user for input/output
struct CS_PipeSSLData {
    SSL *ssl;
};

//SSL pipe pieces...copies from ssl to buffer or buffer to ssl
extern const struct CS_PipeDefinition *CS_PIPE_SSL_IN;
extern const struct CS_PipeDefinition *CS_PIPE_SSL_OUT;

//Compress pipe pieces.
extern const struct CS_PipeDefinition *CS_PIPE_INFLATE;
extern const struct CS_PipeDefinition *CS_PIPE_DEFLATE;
extern const struct CS_PipeDefinition *CS_PIPE_GZIP;
extern const struct CS_PipeDefinition *CS_PIPE_GUNZIP;

//Null pipe. Copies from the in side to its buffer.
extern const struct CS_PipeDefinition *CS_PIPE_NULL;

//Global init and kill for pipe storage
bool CS_pipeInitPipes( int32_t initialPipes );
bool CS_pipeDestroyPipes( void );

struct CS_Pipe *CS_pipeCreate( const struct CS_PipeDefinition *pipeType,
                               int32_t bufferSize,
                               void *pipeData );
int32_t CS_pipeProcess( struct CS_Pipe *anyStage );
int32_t CS_pipeStatus( struct CS_Pipe *anyStage );
int32_t CS_pipeClose( struct CS_Pipe *anyStage );
int32_t CS_pipeFree( struct CS_Pipe *anyStage );
bool CS_pipeHook( struct CS_Pipe *in, struct CS_Pipe *out );
struct CS_Pipe *CS_pipeReconnect( struct CS_Pipe *in, struct CS_Pipe *out);
bool CS_pipeDoneOrError( struct CS_Pipe *anyStage );

#ifdef __cplusplus
}
#endif
#endif
