#ifndef __crankshaftpushpulldoth__
#define __crankshaftpushpulldoth__
#include <stdbool.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
    

/*
 * CS_PushPullBuffer
 *
 * Push pull buffer for io.
 *
 * Data comes in at buffer + readOffset and is written from buffer + writeOffset
 *
 * So, readOffset should always be >= writeOffset
 *
 */

struct CS_PushPullBuffer {
    int32_t size;
    int32_t err;
    int32_t currentReadOffset;
    int32_t currentWriteOffset;
    char *buff;
};

/**********************************
 *
 * Creation methods. Default uses CS_alloc
 * 
 * Allocates a single block with the buffer off the end
 * and initializes the buff pointer.
 *
 * The static buffer method does not allocate the sized
 * buffer, but initializes everything else.
 *
 * The init just initializes the buffer, assuming  you
 * know what you are doing and have allocated one
 *
 **********************************/
struct CS_PushPullBuffer *CS_PP_defaultAlloc(int32_t initialSize);
struct CS_PushPullBuffer *CS_PP_onStaticBuffer(int32_t initialSize, char *buff);
struct CS_PushPullBuffer *CS_PP_fromFile(char *fileName);
void CS_PP_init(struct CS_PushPullBuffer *initMe, int32_t initialSize, char *buff);

void CS_PP_defaultFree(struct CS_PushPullBuffer *freeMe);

#define CS_PP_startOfData(PPBUFF) ((PPBUFF)->buff + (PPBUFF)->currentWriteOffset)
#define CS_PP_endOfData(PPBUFF) ((PPBUFF)->buff + (PPBUFF)->currentReadOffset)
#define CS_PP_dataSize(PPBUFF) ((PPBUFF)->currentReadOffset - (PPBUFF)->currentWriteOffset)
#define CS_PP_bufferRemaining(PPBUFF) ((PPBUFF)->size - (PPBUFF)->currentReadOffset)

int32_t CS_PP_readFromFile(struct CS_PushPullBuffer *buffer, int32_t fileDescriptor);
int32_t CS_PP_readFromBuffer(struct CS_PushPullBuffer *buffer, const void *source, int32_t nBytes);
int32_t CS_PP_readFromSSL(struct CS_PushPullBuffer *buffer, SSL *ssl);
int32_t CS_PP_readFromFILE(struct CS_PushPullBuffer *buffer, FILE *file);
int32_t CS_PP_readFromSocket(struct CS_PushPullBuffer *buffer, int32_t socket);
#define CS_PP_read(PPbuff,PPnBytes) CS_PP_readFromBuffer(PPbuff,NULL,PPnBytes)

int32_t CS_PP_writeToFile(struct CS_PushPullBuffer *buffer, int32_t fileDescriptor);
int32_t CS_PP_writeToBuffer(struct CS_PushPullBuffer *buffer, void *destination, int32_t nBytes);
int32_t CS_PP_writeToSSL(struct CS_PushPullBuffer *buffer, SSL *ssl);
int32_t CS_PP_writeToFILE(struct CS_PushPullBuffer *buffer, FILE *file);
int32_t CS_PP_writeToSocket(struct CS_PushPullBuffer *buffer, int32_t socket);
#define CS_PP_write(PPbuff,PPnBytes) CS_PP_writeToBuffer(PPbuff,NULL,PPnBytes)

#define CS_PP_setFull(PPBUFF) ((PPBUFF)->currentReadOffset=(PPBUFF)->size)
#define CS_PP_rewind(PPBUFF) ((PPBUFF)->currentWriteOffset = 0)
#define CS_PP_hasError(PPBUFF) ((PPBUFF)->err!=0)
#define CS_PP_reset(PPBUFF) ((PPBUFF)->currentWriteOffset=(PPBUFF)->currentReadOffset=0)

bool CS_PP_removeOffEnd( struct CS_PushPullBuffer *buffer, int32_t nBytes );
bool CS_PP_removeChunk( struct CS_PushPullBuffer *buffer, int32_t offset, int32_t nBytes );
bool CS_PP_makeRoom( struct CS_PushPullBuffer *buffer );
char *CS_PP_findChar( struct CS_PushPullBuffer *buffer, char needle );

//Puts toothpaste back in the tube... rewinds nBytes. If the entire buffer has
//been consumed already, will just set it so that it has nBytes in it.
//Assumes you know what you are doing. With great power etc.
bool CS_PP_toothpaste( struct CS_PushPullBuffer *buffer, int32_t nBytes );

//Moves as much as possible from the source to the destination.
int32_t CS_PP_moveBuffer( struct CS_PushPullBuffer *source, struct CS_PushPullBuffer *destination );
int32_t CS_PP_moveBufferExplicit( struct CS_PushPullBuffer *source, struct CS_PushPullBuffer *destination, int32_t max );

#define CS_PP_printf(PPbuff,...) CS_PP_read(PPbuff,snprintf((char*)CS_PP_endOfData(PPbuff),CS_PP_bufferRemaining(PPbuff),__VA_ARGS__));

const char *CS_PP_desc(struct CS_PushPullBuffer *ppBuff);

#ifdef __cplusplus
}
#endif
#endif
