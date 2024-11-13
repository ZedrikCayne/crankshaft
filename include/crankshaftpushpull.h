#ifndef __crankshaftpushpulldoth__
#define __crankshaftpushpulldoth__
#include <stdbool.h>
#include <unistd.h>
#include <openssl/ssl.h>
#ifdef __cplusplus
extern "C" {
#endif
    

/*
 * CS_PushPullBuffer
 *
 * Push pull buffer for io.
 *
 */

struct CS_PushPullBuffer {
    int size;
    int err;
    int currentReadOffset;
    int currentWriteOffset;
    char *buff;
};

/**********************************
 *
 * Creation methods. Default malloc.
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
struct CS_PushPullBuffer *CS_PP_defaultAlloc(int initialSize);
struct CS_PushPullBuffer *CS_PP_onStaticBuff(int initialSize, char *buff);
void CS_PP_init(struct CS_PushPullBuffer *initMe, int initialSize, char *buff);

void CS_PP_defaultFree(struct CS_PushPullBuffer *freeMe);

#define CS_PP_startOfData(PPBUFF) (PPBUFF->buff + PPBUFF->currentWriteOffset)
#define CS_PP_endOfData(PPBUFF) (PPBUFF->buff + PPBUFF->currentReadOffset)
#define CS_PP_dataSize(PPBUFF) (PPBUFF->currentReadOffset - PPBUFF->currentWriteOffset)
#define CS_PP_bufferRemaining(PPBUFF) (PPBUFF->size - PPBUFF->currentReadOffset)

int CS_PP_readFromFile(struct CS_PushPullBuffer *buffer, int fileDescriptor);
int CS_PP_readFromBuffer(struct CS_PushPullBuffer *buffer, const void *source, int nBytes);
int CS_PP_readFromSSL(struct CS_PushPullBuffer *buffer, SSL *ssl);
#define CS_PP_read(PPbuff,PPnBytes) CS_PP_readFromBuffer(PPbuff,NULL,PPnBytes)

int CS_PP_writeToFile(struct CS_PushPullBuffer *buffer, int fileDescriptor);
int CS_PP_writeToBuffer(struct CS_PushPullBuffer *buffer, void *destination, int nBytes);
int CS_PP_writeToSSL(struct CS_PushPullBuffer *buffer, SSL *ssl);
#define CS_PP_write(PPbuff,PPnBytes) CS_PP_writeToBuffer(PPbuff,NULL,PPnBytes)

#define CS_PP_setFull(PPBUFF) (PPBUFF->currentReadOffset=PPBUFF->size)
#define CS_PP_rewind(PPBUFF) (PPBUFF->currentWriteOffset = 0)
#define CS_PP_hasError(PPBUFF) (PPBUFF->err!=0)
#define CS_PP_reset(PPBUFF) (PPBUFF->currentWriteOffset=PPBUFF->currentReadOffset=0)

#define CS_PP_printf(PPbuff,...) CS_PP_read(PPbuff,snprintf((char*)CS_PP_endOfData(PPbuff),CS_PP_bufferRemaining(PPbuff),__VA_ARGS__));

const char *CS_PP_desc(struct CS_PushPullBuffer *ppBuff);

#ifdef __cplusplus
}
#endif
#endif
