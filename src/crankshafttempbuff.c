#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include "crankshaftalloc.h"
#include "crankshafttempbuff.h"
#include "crankshaftlogger.h"

struct TempBuffStorage {
    char name[CS_MAX_TEMP_BUFF_TEMP_NAME];
    char *buffer;
    char *current;
    char *end;
    int size;
    pthread_mutex_t storageMutex;
};

static bool initTempBuff(struct TempBuffStorage *storage,
                         const char *name,
                         int totalSize ) {
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
    strncpy(storage->name, name, CS_MAX_TEMP_BUFF_TEMP_NAME);
    storage->size = totalSize;
    storage->current = buffer;
    storage->buffer = buffer;
    storage->end = buffer + totalSize;
    return false;
}

static void freeTempBuff(struct TempBuffStorage *storage ) {
    if( storage != NULL ) {
        pthread_mutex_destroy(&storage->storageMutex);
        if( storage->buffer != NULL ) CS_free(storage->buffer);
        storage->current = NULL;
        storage->buffer = NULL;
        storage->end = NULL;
        storage->size = 0;
    }
}

static struct TempBuffStorage _TempBuffStorage = {0};

static void *privateAllocate( struct TempBuffStorage *buffer, int size ) {
    int realSize = size % CS_TEMPBUFF_ALIGNMENT == 0 ?
                   size :
                   size + (CS_TEMPBUFF_ALIGNMENT - (size % CS_TEMPBUFF_ALIGNMENT ));
    if( realSize > buffer->size ) return NULL;
    pthread_mutex_lock(&buffer->storageMutex);
    if( buffer->current + realSize > buffer->end ) {
        CS_LOG_TRACE("Temp Buff %s cycled", buffer->name);
        buffer->current = buffer->buffer + realSize;
        return buffer->buffer;
    }
    void *returnValue = buffer->current;
    buffer->current += realSize;
    pthread_mutex_unlock(&buffer->storageMutex);
    return returnValue;
}

void *CS_tempBuff(int size) {
    if( size < 0 || size > CS_TEMPBUFF_MAX_SIZE ) {
        CS_LOG_ERROR( "Not a valid size for a temp buffer allocation %d", size );
        return NULL;
    }
    void *returnValue = privateAllocate( &_TempBuffStorage, size );
    CS_LOG_ERROR_IF(returnValue==NULL,"Attempted to ask for a buffer sized %d. Bigger than the biggest temp buff we have.", size);
    return returnValue;
}

char *CS_tempStringCopy(const char *copyFrom) {
    int nLen = strlen(copyFrom) + 1;
    char *returnValue = CS_tempBuff( nLen );
    if( returnValue ) {
        memcpy( returnValue, copyFrom, nLen );
    }
    return returnValue;
}

void *CS_tempMemCopy( const void *from, int size ) {
    void *returnValue = CS_tempBuff(size);
    if( returnValue ) {
        memcpy(returnValue, from, size);
    }
    return returnValue;
}

bool CS_tempAllocateGlobal(int globalSize) {
    CS_LOG_TRACE("Temp buffers allocated with %d", globalSize);
        
    char tempBufferName[CS_MAX_TEMP_BUFF_TEMP_NAME];
    snprintf(tempBufferName,
             CS_MAX_TEMP_BUFF_TEMP_NAME,
             "Default Temp Buff: %d bytes",
             globalSize );
    return initTempBuff(&_TempBuffStorage,
                     tempBufferName,
                     globalSize );
}

bool CS_tempFreeGlobal() {
    if( _TempBuffStorage.size != 0 ) {
        freeTempBuff(&_TempBuffStorage);
    }
    return false;
}

char *CS_tempBuffSnprintf(int max, char *fmt, ...) {
    char *tBuff = CS_tempBuff(max);
    if( tBuff ) {
        va_list ap;
        va_start( ap, fmt );
        int endy = vsnprintf( (char*)tBuff, max, fmt, ap );
        va_end( ap );
        if( endy > max ) tBuff[max - 1] = 0;
    }
    return tBuff;
}

void *CS_tempAllocManual(const char *name, int size ) {
    if( (name == NULL) || (strlen(name) > CS_MAX_TEMP_BUFF_TEMP_NAME-1) ) {
        CS_LOG_ERROR("Trying to create a temporary buffer stack with a bad name. (Must not be null or longer than %d bytes)", CS_MAX_TEMP_BUFF_TEMP_NAME-1);
        return NULL;
    }
    if( size < 0 ) {
        CS_LOG_ERROR("Trying to create a temporary buffer stack with non positive size");
        return NULL;
    }
    struct TempBuffStorage *returnValue = CS_alloc(sizeof(struct TempBuffStorage));
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

void CS_tempFreeManual(void *manualTempBuff) {
    struct TempBuffStorage *tbuff = (struct TempBuffStorage *)manualTempBuff;
    CS_LOG_TRACE("Freeing a manual temp buffer %s at %p", tbuff->name, tbuff );
    freeTempBuff(tbuff);
    CS_free(manualTempBuff);
}

void *CS_tempGetManual(void *manualTempBuff, int size) {
    if( manualTempBuff == NULL ) return NULL;
    struct TempBuffStorage *storage = (struct TempBuffStorage *)manualTempBuff;
    return privateAllocate( storage, size );
}
