#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <crankshaft/alloc.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/logger.h>
#include <stdint.h>

struct CS_TempBuffer {
    char name[CS_MAX_TEMP_BUFF_TEMP_NAME];
    char *buffer;
    char *current;
    char *end;
    int32_t size;
    pthread_mutex_t storageMutex;
};

static bool initTempBuff(struct CS_TempBuffer *storage,
                         const char *name,
                         int32_t totalSize ) {
    CS_LOG_TRACE("Creating temporary buffer stack '%s'", name);
    if( pthread_mutex_init(&storage->storageMutex, NULL) != 0 ) {
        CS_LOG_ERROR("Cannot create mutex for temp buff named %s", name);
        return true;
    }
    void *buffer = CS_alloc( totalSize );
    if( buffer == NULL ) {
        pthread_mutex_destroy(&storage->storageMutex);
        CS_LOG_ERROR("OOM for temp buff named '%s'", name);
        return true;
    };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(storage->name, name, CS_MAX_TEMP_BUFF_TEMP_NAME-1);
#pragma GCC diagnostic pop
    storage->size = totalSize;
    storage->current = buffer;
    storage->buffer = buffer;
    storage->end = buffer + totalSize;
    return false;
}

static void freeTempBuff(struct CS_TempBuffer *storage ) {
    if( storage != NULL ) {
        pthread_mutex_destroy(&storage->storageMutex);
        if( storage->buffer != NULL ) CS_free(storage->buffer);
        storage->current = NULL;
        storage->buffer = NULL;
        storage->end = NULL;
        storage->size = 0;
    }
}

static struct CS_TempBuffer TempBufferStorage = {0};

static void *privateAllocate( struct CS_TempBuffer *buffer, int32_t size, int32_t align ) {
    int32_t realSize = size % align == 0 ?
                   size :
                   size + (align - (size % align));
    if( realSize > buffer->size ) return NULL;
    pthread_mutex_lock(&buffer->storageMutex);
    if( buffer->current + realSize > buffer->end ) {
        buffer->current = buffer->buffer;
    }
    void *returnValue = buffer->current;
    buffer->current += realSize;
    pthread_mutex_unlock(&buffer->storageMutex);
    return returnValue;
}

void *CS_tempBuff(int32_t size) {
    if( size < 0 || size > CS_TEMPBUFF_MAX_SIZE ) {
        CS_LOG_ERROR( "Not a valid size for a temp buffer allocation %d", size );
        return NULL;
    }
    void *returnValue = privateAllocate( &TempBufferStorage, size, CS_TEMPBUFF_ALIGNMENT );
    CS_LOG_ERROR_IF(returnValue==NULL,"Attempted to ask for a buffer sized %d. Bigger than the biggest temp buff we have.", size);
    return returnValue;
}

void *CS_tempBuffZero( int32_t size ) {
    void *returnValue = CS_tempBuff(size);
    if( returnValue ) memset( returnValue, 0, size );
    return returnValue;
}

char *CS_tempCstringCopy(const char *copyFrom) {
    int32_t nLen = strlen(copyFrom) + 1;
    char *returnValue = CS_tempBuff( nLen );
    if( returnValue ) {
        memcpy( returnValue, copyFrom, nLen );
    }
    return returnValue;
}

char *CS_tempCstringCopyWithPad(const char *copyFrom, int32_t size, char pad, int32_t *outLength, int32_t aligned) {
    int32_t nLen = size;
    int32_t newLength = size%aligned==0?size:(size + aligned - ( size % aligned ) );
    char *returnValue = CS_tempBuffZero( newLength + 1 );
    if( returnValue ) {
        strncpy( returnValue, copyFrom, size );
        int32_t oldLen = strlen( returnValue );
        if( oldLen < size ) {
            nLen = oldLen;
            newLength = oldLen%aligned==0?oldLen:(oldLen + aligned - (oldLen % aligned));
        }
        for( int32_t i = nLen; i < newLength; ++i ) returnValue[ i ] = pad;
        returnValue[newLength] = 0;
    }
    if( outLength ) *outLength = newLength;

    return returnValue;
}

void *CS_tempMemCopy( const void *from, int32_t size ) {
    void *returnValue = CS_tempBuff(size);
    if( returnValue ) {
        memcpy(returnValue, from, size);
    }
    return returnValue;
}

bool CS_tempAllocateGlobal(int32_t globalSize) {
    if( TempBufferStorage.size == 0 ) {
        CS_LOG_TRACE("Temp buffers allocated size %d", globalSize);
            
        char tempBufferName[CS_MAX_TEMP_BUFF_TEMP_NAME];
        snprintf(tempBufferName,
                 CS_MAX_TEMP_BUFF_TEMP_NAME,
                 "Default Temp Buff: %d bytes",
                 globalSize );
        return initTempBuff(&TempBufferStorage,
                         tempBufferName,
                         globalSize );
    }
    return false;
}

bool CS_tempFreeGlobal() {
    if( TempBufferStorage.size != 0 ) {
        CS_LOG_TRACE("Temp buffers de-allocated size %d", TempBufferStorage.size);
        freeTempBuff(&TempBufferStorage);
    }
    return false;
}

char *CS_tempBuffSnprintf(int32_t max, const char *fmt, ...) {
    char *tBuff = CS_tempBuff(max);
    if( tBuff ) {
        va_list ap;
        va_start( ap, fmt );
        int32_t endy = vsnprintf( (char*)tBuff, max, fmt, ap );
        va_end( ap );
        if( endy > max ) tBuff[max - 1] = 0;
    }
    return tBuff;
}

struct CS_TempBuffer *CS_tempAllocManual(const char *name, int32_t size ) {
    if( (name == NULL) || (strlen(name) > CS_MAX_TEMP_BUFF_TEMP_NAME-1) ) {
        CS_LOG_ERROR("Trying to create a temporary buffer stack with a bad name. (Must not be null or longer than %d bytes)", CS_MAX_TEMP_BUFF_TEMP_NAME-1);
        return NULL;
    }
    if( size < 0 ) {
        CS_LOG_ERROR("Trying to create a temporary buffer stack with non positive size");
        return NULL;
    }
    struct CS_TempBuffer *returnValue = CS_alloc(sizeof(struct CS_TempBuffer));
    if( returnValue == NULL ) {
        CS_LOG_ERROR("OOM allocating a temporary buffer storage named %s.", name);
        return NULL;
    }
    if( initTempBuff( returnValue, name, size ) ) {
        CS_free(returnValue);
        return NULL;
    }
    CS_LOG_TRACE("Created a manual temp buffer %s at %p", name, returnValue);
    return returnValue;
}

void CS_tempFreeManual(struct CS_TempBuffer *manualTempBuff) {
    CS_LOG_TRACE("Freeing a manual temp buffer %s at %p", manualTempBuff->name, manualTempBuff );
    freeTempBuff(manualTempBuff);
    CS_free(manualTempBuff);
}

void *CS_tempGetManual(struct CS_TempBuffer *manualTempBuff, int32_t size, int32_t align) {
    if( manualTempBuff == NULL ) return NULL;
    return privateAllocate( manualTempBuff, size, align );
}
