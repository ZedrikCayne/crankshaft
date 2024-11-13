#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "crankshaftalloc.h"
#include "crankshafttempbuff.h"
#include "crankshaftlogger.h"

struct TempBuffStorage {
    char name[CS_MAX_TEMP_BUFF_TEMP_NAME];
    int size;
    int current;
    int capacity;
    char *buffer;
    pthread_mutex_t storageMutex;
};

struct TempBuffStorageStorage {
    int size;
    int capacity;
    struct TempBuffStorage *bufferStorage;
};

static bool initTempBuff(struct TempBuffStorage *storage,
                         const char *name,
                         int elementSize,
                         int numberOfElements,
                         int alignment ) {
    int realElementSize = elementSize % alignment == 0 ?
                          elementSize :
                          elementSize + (alignment - (elementSize % alignment));
    CS_LOG_TRACE("Creating temporary buffer stack '%s'", name);
    if( pthread_mutex_init(&storage->storageMutex, NULL) != 0 ) {
        CS_LOG_ERROR("Cannot create mutex for temp buff named %s", name);
        return true;
    }
    void *buffer = CS_alloc( realElementSize * numberOfElements );
    if( buffer == NULL ) {
        pthread_mutex_destroy(&storage->storageMutex);
        CS_LOG_ERROR("OOM for temp buff named '%s'", name);
        return true;
    };
    strncpy(storage->name, name, CS_MAX_TEMP_BUFF_TEMP_NAME);
    storage->size = realElementSize;
    storage->current = 0;
    storage->capacity = numberOfElements;
    storage->buffer = buffer;
    return false;
}

static void freeTempBuff(struct TempBuffStorage *storage ) {
    if( storage != NULL && storage->buffer != NULL ) {
        pthread_mutex_destroy(&storage->storageMutex);
        CS_free(storage->buffer);
        storage->buffer = NULL;
    }
}

//A meg each should be big enough? Right?
static struct TempBuffStorageStorage _TempBuffStorage = {0};

void *CS_tempBuff(int size) {
    void *returnValue = NULL;
    struct TempBuffStorage *current = _TempBuffStorage.bufferStorage;
    for( int i = 0; i < _TempBuffStorage.size; ++i ) {
        if( size < current->size ) {
            returnValue = CS_getTempBuff(current);
            break;
        }
        ++current;
    }
    CS_LOG_ERROR_IF(returnValue==NULL,"Attempted to ask for a buffer sized %d. Bigger than the biggest temp buff we have.", size);
    return returnValue;
}

bool CS_allocateTempBuffs() {
    if( _TempBuffStorage.size == 0 ) {
        int sizeThing = CS_MIN_TEMP_BUFF_SIZE;
        int numberOfBuffers = 0;
        while( sizeThing <= CS_MAX_TEMP_BUFF_SIZE ) {
            ++numberOfBuffers;
            sizeThing = CS_MIN_TEMP_BUFF_SIZE << numberOfBuffers;
        }
        CS_LOG_TRACE("Number of temporary buffers wanted %d", numberOfBuffers);
        
        _TempBuffStorage.size = numberOfBuffers;
        _TempBuffStorage.bufferStorage = CS_alloc( sizeof( struct TempBuffStorage ) * _TempBuffStorage.size );
        if( _TempBuffStorage.bufferStorage == NULL ) {
            CS_LOG_ERROR("Failed to allocate global temp buffs. Things will go badly from here on out.");
            goto ALLOCATE_ERROR;
        }
        memset( _TempBuffStorage.bufferStorage, 0, sizeof( struct TempBuffStorage ) * _TempBuffStorage.size ); 
    }
    char tempBufferName[CS_MAX_TEMP_BUFF_TEMP_NAME];
    for( int i = 0; i < _TempBuffStorage.size; ++i ) {
        int currentBuffSize = CS_MIN_TEMP_BUFF_SIZE << i;
        int capacity = CS_TEMP_BUFF_ALLOC_SIZE / currentBuffSize;
        snprintf(tempBufferName,
                 CS_MAX_TEMP_BUFF_TEMP_NAME,
                 "Default Temp Buff: %d buffers of %d bytes",
                 capacity, currentBuffSize);
        if( initTempBuff(_TempBuffStorage.bufferStorage + i,
                         tempBufferName,
                         currentBuffSize,
                         capacity,
                         CS_TEMP_BUFF_ALIGNMENT ) ) {
            goto ALLOCATE_ERROR_WITH_FREE;
        }
    }
    return false;
ALLOCATE_ERROR_WITH_FREE:
    CS_freeAllTempBuffs();
ALLOCATE_ERROR:
    _TempBuffStorage.size = 0;
    return true;
}

bool CS_freeAllTempBuffs() {
    if( _TempBuffStorage.size != 0 ) {
        struct TempBuffStorage *current = _TempBuffStorage.bufferStorage;
        for( int i = 0; i < _TempBuffStorage.size; ++i ) {
            freeTempBuff(current);
            ++current;
        }
        _TempBuffStorage.size = 0;
        CS_free( _TempBuffStorage.bufferStorage );
    }
    return false;
}

void *CS_allocManualTempBuff(const char *name,
                             int elementSize,
                             int numberOfElements,
                             int alignment) {
    if( (name == NULL) || (strlen(name) > CS_MAX_TEMP_BUFF_TEMP_NAME-1) ) {
        CS_LOG_ERROR("Trying to create a temporary buffer stack with a bad name. (Must not be null or longer than %d bytes)", CS_MAX_TEMP_BUFF_TEMP_NAME-1);
        return NULL;
    }
    if( elementSize <= 0 || numberOfElements <= 0 || alignment <= 0 ) {
        CS_LOG_ERROR("Trying to create a temporary buffer stack with non positive elementSize, numberOfElements or alignment.");
        return NULL;
    }
    struct TempBuffStorage *returnValue = CS_alloc(sizeof(struct TempBuffStorage));
    if( returnValue == NULL ) {
        CS_LOG_ERROR("OOM allocating a temporary buffer storage named %s.", name);
        return NULL;
    }
    if( initTempBuff( returnValue, name, elementSize, numberOfElements, alignment ) ) {
        CS_free(returnValue);
        return NULL;
    }
    CS_LOG_TRACE("Created a manual temp buffer %s at %p", name, returnValue);
    return returnValue;
}

void CS_freeManualTempBuff(void *manualTempBuff) {
    struct TempBuffStorage *tbuff = (struct TempBuffStorage *)manualTempBuff;
    CS_LOG_TRACE("Freeing a manual temp buffer %s at %p", tbuff->name, tbuff );
    freeTempBuff(tbuff);
    CS_free(manualTempBuff);
}

void *CS_getTempBuff(void *manualTempBuff) {
    if( manualTempBuff == NULL ) return NULL;
    struct TempBuffStorage *storage = (struct TempBuffStorage *)manualTempBuff;
    if( storage->capacity == 0 ) return NULL;
    pthread_mutex_lock(&storage->storageMutex);
    void *returnValue = storage->buffer + (storage->current*storage->size);
    storage->current = (storage->current + 1) % storage->capacity;
    CS_LOG_TRACE_IF( storage->current==0, "Temp buffer %s rolled over.", storage->name);
    pthread_mutex_unlock(&storage->storageMutex);
    return returnValue;
}
