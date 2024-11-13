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

struct CS_StringBuilder *CS_SB_printf( struct CS_StringBuilder *buffer, const char *fmt, ... ) {
    int bytesNeeded = 0;
    va_list ap;

ONCE_MORE_UNTO_THE_BREACH:
    va_start(ap,fmt);
    bytesNeeded = vsnprintf(buffer->buffer + buffer->currentHead, buffer->currentSize - buffer->currentHead, fmt, ap );
    va_end(ap);
    if( bytesNeeded >= buffer->currentSize - buffer->currentHead ) {
        if( expandIfNeeded(buffer, bytesNeeded) ) return NULL;
        goto ONCE_MORE_UNTO_THE_BREACH;
    }

    buffer->currentHead += bytesNeeded;
    buffer->buffer[ buffer->currentHead ] = 0;
    return buffer;
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
            snprintf(tbuff, MAX_PRINT_LENGTH, "StringBuffer: %d %d %d 0x%p", buffer->originalSize, buffer->currentSize, buffer->currentHead, buffer->buffer );
        }
    }
    return tbuff;
}
