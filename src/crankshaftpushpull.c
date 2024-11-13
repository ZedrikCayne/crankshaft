#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <openssl/ssl.h>

#include "crankshaftalloc.h"
#include "crankshaftpushpull.h"
#include "crankshafttempbuff.h"

#define CS_PP_bytesRequired(x) (sizeof(struct CS_PushPullBuffer)+x)

static struct CS_PushPullBuffer *internalInitAndAlloc(int initialSize, bool bufferPreInitialized, char *buff);

struct CS_PushPullBuffer *CS_PP_defaultAlloc(int initialSize) {
    return internalInitAndAlloc(initialSize, false, NULL);
}

struct CS_PushPullBuffer *CS_PP_onStaticBuffer(int initialSize, char *buff) {
    return internalInitAndAlloc(initialSize, true, buff);
}

void CS_PP_init(struct CS_PushPullBuffer *buffer, int initialSize, char *buff) {
    buffer->size = initialSize;
    buffer->err = 0;
    buffer->currentReadOffset = 0;
    buffer->currentWriteOffset = 0;
    buffer->buff = buff;
}

static struct CS_PushPullBuffer *internalInitAndAlloc(int initialSize, bool bufferPreInitialized, char *buff) {
    int bytesRequired = buff==NULL?CS_PP_bytesRequired(initialSize):sizeof(struct CS_PushPullBuffer);
    struct CS_PushPullBuffer *returnValue = CS_alloc(bytesRequired);
    if( returnValue == NULL ) {
        return NULL;
    }
    char *realBuff = buff;
    if( realBuff == NULL )
        realBuff = (char *)(returnValue + 1);
    CS_PP_init(returnValue, initialSize, realBuff);
    if( bufferPreInitialized ) CS_PP_setFull(returnValue);
    return returnValue;
}

void CS_PP_defaultFree(struct CS_PushPullBuffer *freeMe) {
    CS_free(freeMe);
}

int CS_PP_readFromFile(struct CS_PushPullBuffer *buffer, int fileDescriptor) {
    buffer->err = 0;
    if( buffer->currentReadOffset < buffer->size ) {
        int bytesRead = read( fileDescriptor,
                              CS_PP_endOfData(buffer),
                              CS_PP_bufferRemaining(buffer) );
        if( bytesRead < 0 || (bytesRead == 0 && errno != 0) ) {
            buffer->err = errno;
        } else {
            buffer->currentReadOffset += bytesRead;
        }
        return bytesRead;
    }
    return 0;
}

int CS_PP_writeToFile(struct CS_PushPullBuffer *buffer, int fileDescriptor) {
    buffer->err = 0;
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        int bytesWritten = write( fileDescriptor,
                                  CS_PP_startOfData(buffer),
                                  CS_PP_dataSize(buffer) );
        if( bytesWritten < 0 || (bytesWritten == 0 && errno != 0) ) {
            buffer->err = errno;
        } else {
            buffer->currentWriteOffset += bytesWritten;
        }
        if( buffer->currentWriteOffset == buffer->currentReadOffset ) {
            buffer->currentWriteOffset = buffer->currentReadOffset = 0;
        }
        return bytesWritten;
    }
    return 0;
}

int CS_PP_readFromBuffer(struct CS_PushPullBuffer *buffer, const void *source, int nBytes) {
    if( buffer->currentReadOffset < buffer->size ) {
        int bytesPulled = nBytes;
        if( buffer->currentReadOffset + nBytes > buffer->size ) {
            bytesPulled = buffer->size - buffer->currentReadOffset;
        }
        if( source != NULL ) memcpy( buffer->buff + buffer->currentReadOffset, source, bytesPulled );
        buffer->currentReadOffset += bytesPulled;
        return bytesPulled;
    }
    return 0;
}

int CS_PP_writeToBuffer(struct CS_PushPullBuffer *buffer, void *destination, int nBytes) {
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        int bytesPushed = nBytes;
        if( buffer->currentWriteOffset + nBytes >= buffer->currentReadOffset ) {
            bytesPushed = buffer->currentReadOffset - buffer->currentWriteOffset;
        }
        if( destination != NULL ) memcpy( destination, buffer->buff + buffer->currentWriteOffset, bytesPushed );
        buffer->currentWriteOffset += bytesPushed;
        if( buffer->currentWriteOffset == buffer->currentReadOffset ) {
            buffer->currentWriteOffset = buffer->currentReadOffset = 0;
        }
        return bytesPushed;
    }
    return 0;
}

 int CS_PP_readFromSSL(struct CS_PushPullBuffer *buffer, SSL *ssl) {
    buffer->err = 0;
    if( buffer->currentReadOffset < buffer->size ) {
        int bytesRead = SSL_read( ssl,
                                  CS_PP_endOfData(buffer),
                                  CS_PP_bufferRemaining(buffer) );
        if( bytesRead < 0 || (bytesRead == 0 && errno != 0) ) {
            buffer->err = errno;
        } else {
            buffer->currentReadOffset += bytesRead;
        }
        return bytesRead;
    }
    return 0;
}

int CS_PP_writeToSSL(struct CS_PushPullBuffer *buffer, SSL *ssl) {
    buffer->err = 0;
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        int bytesWritten = SSL_write( ssl,
                                      CS_PP_startOfData(buffer),
                                      CS_PP_dataSize(buffer) );
        if( bytesWritten < 0 || (bytesWritten == 0 && errno != 0) ) {
            buffer->err = errno;
        } else {
            buffer->currentWriteOffset += bytesWritten;
        }
        if( buffer->currentWriteOffset == buffer->currentReadOffset ) {
            buffer->currentWriteOffset = buffer->currentReadOffset = 0;
        }
        return bytesWritten;
    }
    return 0;
}

#define MAX_PRINT_SIZE 1024
const char *CS_PP_desc(struct CS_PushPullBuffer *buffer) {
    char *temp = (char*)CS_tempBuff(MAX_PRINT_SIZE);
    snprintf(temp, MAX_PRINT_SIZE, "CS_PushPullBuffer( buff: %p, size: %d, readOffset: %d, writeOffset: %d, dataSize: %d, bufferRemaining: %d  )",
            buffer->buff,
            buffer->size,
            buffer->currentReadOffset,
            buffer->currentWriteOffset,
            CS_PP_dataSize( buffer ),
            CS_PP_bufferRemaining( buffer ) );
    return temp;
}
