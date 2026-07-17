#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <crankshaft/alloc.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/tempbuff.h>
#include <stdint.h>

#define CS_PP_bytesRequired(x) (sizeof(struct CS_PushPullBuffer)+x)

static struct CS_PushPullBuffer *internalInitAndAlloc(int32_t initialSize, bool bufferPreInitialized, char *buff);

struct CS_PushPullBuffer *CS_PP_defaultAlloc(int32_t initialSize) {
    return internalInitAndAlloc(initialSize, false, NULL);
}

struct CS_PushPullBuffer *CS_PP_onStaticBuffer(int32_t initialSize, char *buff) {
    return internalInitAndAlloc(initialSize, true, buff);
}

struct CS_PushPullBuffer *CS_PP_fromFile(char *fileName) {
    struct stat fileStat = {0};
    if( stat(fileName, &fileStat) != 0 ) return NULL;
    struct CS_PushPullBuffer *returnValue = NULL;

    int32_t readFile = open(fileName,0);
    if( readFile >= 0 ) {
        returnValue = CS_PP_defaultAlloc( fileStat.st_size );
        if( returnValue ) {
            if( CS_PP_readFromFile( returnValue, readFile ) < 0 ) {
                CS_PP_defaultFree( returnValue );
                returnValue = NULL;
            }
        }
        close(readFile);
    }

    return returnValue;
}

void CS_PP_init(struct CS_PushPullBuffer *buffer, int32_t initialSize, char *buff) {
    buffer->size = initialSize;
    buffer->err = 0;
    buffer->currentReadOffset = 0;
    buffer->currentWriteOffset = 0;
    buffer->buff = buff;
}

static struct CS_PushPullBuffer *internalInitAndAlloc(int32_t initialSize, bool bufferPreInitialized, char *buff) {
    int32_t bytesRequired = buff==NULL?CS_PP_bytesRequired(initialSize):sizeof(struct CS_PushPullBuffer);
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

int32_t CS_PP_readFromFile(struct CS_PushPullBuffer *buffer, int32_t fileDescriptor) {
    buffer->err = 0;
    if( buffer->currentReadOffset < buffer->size ) {
        errno = 0;
        int32_t bytesRead = read( fileDescriptor,
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

int32_t CS_PP_readFromSocket(struct CS_PushPullBuffer *buffer, int32_t fileDescriptor) {
    buffer->err = 0;
    if( buffer->currentReadOffset < buffer->size ) {
        errno = 0;
        int32_t bytesRead = recv( fileDescriptor,
                              CS_PP_endOfData(buffer),
                              CS_PP_bufferRemaining(buffer),
                              0 );
        if( bytesRead < 0 ) {
            buffer->err = errno;
        } else {
            buffer->currentReadOffset += bytesRead;
        }
        return bytesRead;
    }
    return 0;
}

int32_t CS_PP_writeToFile(struct CS_PushPullBuffer *buffer, int32_t fileDescriptor) {
    buffer->err = 0;
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        errno = 0;
        int32_t bytesWritten = write( fileDescriptor,
                                  CS_PP_startOfData(buffer),
                                  CS_PP_dataSize(buffer) );
        if( bytesWritten < 0 || (bytesWritten == 0 && errno != 0) ) {
            buffer->err = errno;
            bytesWritten = -1;
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

int32_t CS_PP_writeToSocket(struct CS_PushPullBuffer *buffer, int32_t socket) {
    return CS_PP_writeToFile(buffer,socket);
}

int32_t CS_PP_readFromBuffer(struct CS_PushPullBuffer *buffer, const void *source, int32_t nBytes) {
    if( buffer->currentReadOffset < buffer->size ) {
        int32_t bytesPulled = nBytes;
        if( buffer->currentReadOffset + nBytes > buffer->size ) {
            bytesPulled = buffer->size - buffer->currentReadOffset;
        }
        if( source != NULL ) memcpy( buffer->buff + buffer->currentReadOffset, source, bytesPulled );
        buffer->currentReadOffset += bytesPulled;
        return bytesPulled;
    }
    return 0;
}

int32_t CS_PP_writeToBuffer(struct CS_PushPullBuffer *buffer, void *destination, int32_t nBytes) {
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        int32_t bytesPushed = nBytes;
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

 int32_t CS_PP_readFromSSL(struct CS_PushPullBuffer *buffer, SSL *ssl) {
    buffer->err = 0;
    if( buffer->currentReadOffset < buffer->size ) {
        errno = 0;
        int32_t bytesRead = SSL_read( ssl,
                                  CS_PP_endOfData(buffer),
                                  CS_PP_bufferRemaining(buffer) );
        if( bytesRead <= 0 ) {
            bytesRead = -1;
            buffer->err = errno;
        } else {
            buffer->currentReadOffset += bytesRead;
        }
        return bytesRead;
    }
    return 0;
}

int32_t CS_PP_writeToSSL(struct CS_PushPullBuffer *buffer, SSL *ssl) {
    buffer->err = 0;
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        errno = 0;
        int32_t bytesWritten = SSL_write( ssl,
                                      CS_PP_startOfData(buffer),
                                      CS_PP_dataSize(buffer) );
        if( bytesWritten < 0 || (bytesWritten == 0 && errno != 0) ) {
            bytesWritten = -1;
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

int32_t CS_PP_readFromFILE(struct CS_PushPullBuffer *buffer, FILE *file) {
    buffer->err = 0;
    if( buffer->currentReadOffset < buffer->size ) {
        errno = 0;
        int32_t bytesRead = fread( CS_PP_endOfData(buffer),
                               1,
                               CS_PP_bufferRemaining(buffer),
                               file );
        if( bytesRead < 0 || (bytesRead == 0 && errno != 0) ) {
            bytesRead = -1;
            buffer->err = errno;
        } else {
            buffer->currentReadOffset += bytesRead;
        }
        return bytesRead;
    }
    return 0;
}

int32_t CS_PP_writeToFILE(struct CS_PushPullBuffer *buffer, FILE *file) {
    buffer->err = 0;
    if( buffer->currentWriteOffset < buffer->currentReadOffset ) {
        errno = 0;
        int32_t bytesWritten = fwrite( CS_PP_startOfData(buffer),
                                   1,
                                   CS_PP_dataSize(buffer),
                                   file );
        if( bytesWritten < 0 || (bytesWritten == 0 && errno != 0) ) {
            bytesWritten = -1;
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

#define MOVE(_PP,_OFFSET,_NUM){char *out=_PP->buff+_PP->currentWriteOffset+_OFFSET;char *end=_PP->buff+_PP->currentReadOffset;char *in=out+_NUM;while(in<end)*out++=*in++;}
#define BACKFILL(_PP,_NUM){char *out=_PP->buff+_PP->currentReadOffset;char *end=out+_NUM;while(out<end)*out++='%';}

bool CS_PP_removeOffEnd( struct CS_PushPullBuffer *buffer, int32_t nBytes ) {
    if( nBytes > CS_PP_dataSize(buffer) ) return true;
    buffer->currentReadOffset -= nBytes;
    BACKFILL(buffer,nBytes);
    return false;
}

bool CS_PP_removeChunk( struct CS_PushPullBuffer *buffer, int32_t offset, int32_t nBytes ) {
    //Can't remove more than we have data available.
    if( nBytes + offset > CS_PP_dataSize(buffer) ) return true;
    //memmove( buffer->buff + buffer->currentWriteOffset + offset,
             //buffer->buff + buffer->currentWriteOffset + offset + nBytes,
             //buffer->currentReadOffset - buffer->currentWriteOffset + offset + nBytes );
    MOVE(buffer,offset,nBytes);
    buffer->currentReadOffset -= nBytes;
    BACKFILL(buffer,nBytes);
    return false;
}

bool CS_PP_makeRoom( struct CS_PushPullBuffer *buffer ) {
    int32_t nBytesToMove = buffer->currentWriteOffset;
    if( nBytesToMove == 0 ) return true;
    //memmove( buffer->buff,
             //buffer->buff + nBytesToMove,
             //nBytesToMove );
    MOVE(buffer,0,nBytesToMove);
    buffer->currentWriteOffset = 0;
    buffer->currentReadOffset -= nBytesToMove;
    return false;
}

char *CS_PP_findChar( struct CS_PushPullBuffer *buffer, char needle ) {
    char *in = buffer->buff + buffer->currentWriteOffset;
    char *end = buffer->buff + buffer->currentReadOffset;
    while( in < end && *in++ != needle );
    return in==end?NULL:in;
}


int32_t CS_PP_moveBuffer( struct CS_PushPullBuffer *source, struct CS_PushPullBuffer *destination ) {
    int32_t sourceSize = CS_PP_dataSize( source );
    if( sourceSize == 0 ) return 0;
    int32_t destinationSize = CS_PP_bufferRemaining( destination );
    if( destinationSize == 0 ) return 0;
    int32_t moveSize = sourceSize < destinationSize?sourceSize:destinationSize;
    //updates the destination pointers.
    CS_PP_readFromBuffer( destination, CS_PP_startOfData( source ), moveSize );
    //updates the source pointers.
    CS_PP_write( source, moveSize );
    return moveSize;
}

int32_t CS_PP_moveBufferExplicit( struct CS_PushPullBuffer *source, struct CS_PushPullBuffer *destination, int32_t max ) {
    int32_t sourceSize = CS_PP_dataSize( source );
    if( sourceSize == 0 ) return 0;
    int32_t destinationSize = CS_PP_bufferRemaining( destination );
    if( destinationSize == 0 ) return 0;
    int32_t moveSize = sourceSize < destinationSize?sourceSize:destinationSize;
    if( max < moveSize ) moveSize = max;
    //updates the destination pointers.
    CS_PP_readFromBuffer( destination, CS_PP_startOfData( source ), moveSize );
    //updates the source pointers.
    CS_PP_write( source, moveSize );
    return moveSize;
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

bool CS_PP_toothpaste( struct CS_PushPullBuffer *buffer, int32_t nBytes ) {
    //Buffer is empty or was reset from reading...just claim it has nBytes in it.
    if( buffer->currentReadOffset == 0 && buffer->currentWriteOffset == 0 ) {
        buffer->currentReadOffset = nBytes;
        return false;
    }
    //We have data, but not enough to put back into the tube... return error.
    if( buffer->currentWriteOffset < nBytes ) {
        return true;
    }
    //Move the write offset back by nBytes.
    buffer->currentWriteOffset -= nBytes;
    return false;
}

