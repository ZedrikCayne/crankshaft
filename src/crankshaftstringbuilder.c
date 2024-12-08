#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftstringbuilder.h"
#include "crankshafttempbuff.h"

struct CS_StringBuilder *CS_SB_create( int initialSize ) {
    struct CS_StringBuilder *returnValue = CS_alloc(sizeof(struct CS_StringBuilder));
    if( returnValue != NULL ) {
        returnValue->buffer = CS_alloc(initialSize);
        if( returnValue->buffer == NULL ) {
            CS_free(returnValue);
            returnValue = NULL;
        } else {
            returnValue->originalSize = returnValue->currentSize = initialSize;
            returnValue->currentHead = 0;
        }
    }
    return returnValue;
}

const bool expandIfNeeded( struct CS_StringBuilder *buffer, int bytesNeeded ) {
    if( buffer->currentHead + bytesNeeded + 1 > buffer->currentSize ) {
        int newSize = buffer->currentSize;
        while( newSize < buffer->currentHead + bytesNeeded + 1 ) newSize+=buffer->originalSize;
        void *newBuff = CS_realloc( buffer->buffer, newSize );
        if( newBuff == NULL ) {
            return true;
        }
        buffer->buffer = newBuff;
        buffer->currentSize = newSize;
    }
    return false;
}

struct CS_StringBuilder *CS_SB_appendChar( struct CS_StringBuilder *buffer, const char ch ) {
    if( expandIfNeeded( buffer, 1 ) ) return NULL;
    buffer->buffer[ buffer->currentHead ] = ch;
    buffer->currentHead++;
    buffer->buffer[ buffer->currentHead ] = 0;
    return buffer;
}

struct CS_StringBuilder *CS_SB_append( struct CS_StringBuilder *buffer, const char *string ) {
    int bytesNeeded = strlen(string);
    if( expandIfNeeded( buffer, bytesNeeded ) ) return NULL;
    memcpy( buffer->buffer + buffer->currentHead, string, bytesNeeded + 1 );
    buffer->currentHead += bytesNeeded;
    buffer->buffer[ buffer->currentHead ] = 0;
    return buffer;
}

bool CS_SB_expandBy( struct CS_StringBuilder *buffer, int minimumNewCapacity ) {
    return expandIfNeeded( buffer, minimumNewCapacity );
}

struct CS_StringBuilder *CS_SB_vsnprintf( struct CS_StringBuilder *buffer, int maxAppend, const char *fmt, va_list ap ) {
    int remain;
    int currentMax;
    va_list apCpy;
    int bytesNeeded;

ONCE_MORE_UNTO_THE_BREACH:
    va_copy(apCpy,ap);
    currentMax = remain = CS_SB_remain(buffer);
    if( maxAppend > 0 && currentMax > maxAppend ) currentMax = maxAppend;
    bytesNeeded = vsnprintf(buffer->buffer + buffer->currentHead, currentMax, fmt, apCpy );
    if( (currentMax > remain ) && (bytesNeeded >= remain) ) {
        if( expandIfNeeded(buffer, bytesNeeded) ) return NULL;
        goto ONCE_MORE_UNTO_THE_BREACH;
    }
    if( maxAppend > 0 && bytesNeeded > currentMax ) {
        //-1 here because the printf above will have terminated the string one byte earlier
        bytesNeeded = currentMax - 1;
    }

    buffer->currentHead += bytesNeeded;
    if( maxAppend > 0 && bytesNeeded > currentMax ) buffer->buffer[ buffer->currentHead ] = 0;
    return buffer;
}

struct CS_StringBuilder *CS_SB_snprintf( struct CS_StringBuilder *buffer, int maxAppend, const char *fmt, ... ) {
    struct CS_StringBuilder *returnValue;
    va_list ap;
    va_start(ap,fmt);
    returnValue = CS_SB_vsnprintf( buffer, maxAppend, fmt, ap );
    va_end(ap);
    return returnValue;
}

void CS_SB_free( struct CS_StringBuilder *buffer ) {
    if( buffer->buffer ) CS_free( buffer->buffer );
    if( buffer ) CS_free( buffer );
}

const char *CS_SB_freeButReturnBuffer( struct CS_StringBuilder *buffer ) {
    const char *returnValue = NULL;
    if( buffer ) {
        returnValue = buffer->buffer;
        CS_free(buffer);
    }
    return returnValue;
}

#define MAX_PRINT_LENGTH 1024
const char *CS_SB_desc( struct CS_StringBuilder *buffer ) {
    char *tbuff = CS_tempBuff(MAX_PRINT_LENGTH);
    if( tbuff ) {
        if( buffer == NULL ) {
            snprintf(tbuff, MAX_PRINT_LENGTH, "StringBuffer: NULL");
        } else {
            snprintf(tbuff, MAX_PRINT_LENGTH, "StringBuffer: %d %d %d %p", buffer->originalSize, buffer->currentSize, buffer->currentHead, buffer->buffer );
        }
    }
    return tbuff;
}
