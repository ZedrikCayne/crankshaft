#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/slaballoc.h>

#include <crankshaft/mutex.h>


struct CS_Mutex {
    char name[ MUTEX_MAX_NAME ];
    const char *file;
    int line;
    bool locked;
    pthread_mutex_t *mutex;
};

static struct CS_SlabAllocator *mutexes = NULL;
static struct CS_SlabAllocator *csMutexes = NULL;

static pthread_mutex_t mutexesMutex = PTHREAD_MUTEX_INITIALIZER;

static bool veryVerboseMutexLogs = false;

void CS_mutexDebug( bool debugMe ) {
    veryVerboseMutexLogs = debugMe;
}

static struct CS_Mutex *privateTakeCSMutex() {
    if( csMutexes == NULL ) {
        pthread_mutex_lock(&mutexesMutex);
        if( csMutexes == NULL ) {
            csMutexes = CS_slabInit( "CSMUTEXES", sizeof( struct CS_Mutex ), 64, sizeof( void * ) );
            if( csMutexes == NULL ) {
                pthread_mutex_unlock( &mutexesMutex );
                return NULL;
            }
        }
        pthread_mutex_unlock(&mutexesMutex);
    }
    return CS_slabTakeZero(csMutexes);
}

static bool privateReturnCSMutex( struct CS_Mutex *returnMe ) {
    if( !csMutexes ) return true;
    CS_slabReturn(csMutexes,returnMe);
    return false;
}

static pthread_mutex_t *privateTakeMutex() {
    if( mutexes == NULL ) {
        pthread_mutex_lock( &mutexesMutex );
        if( mutexes == NULL ) {
            mutexes = CS_slabInit( "MUTEXES", sizeof( pthread_mutex_t ), 64, sizeof( void * ) );
            if( mutexes == NULL ) {
                pthread_mutex_unlock( &mutexesMutex );
                return NULL;
            }
        }
        pthread_mutex_unlock( &mutexesMutex );
    }

    pthread_mutex_t *returnValue = CS_slabTakeZero( mutexes );
    if( !returnValue ) return NULL;
    if( pthread_mutex_init( returnValue, NULL ) ) {
        CS_slabReturn( mutexes, returnValue );
        return NULL;
    }
    return returnValue;
}

static void privateReturnMutex( pthread_mutex_t *mutex ) {
    pthread_mutex_destroy( mutex );
    CS_slabReturn( mutexes, mutex );
}

static void printMutex( struct CS_Mutex *mutex, const char *what, const char *file, int line ) {
    if( veryVerboseMutexLogs ) CS_LOG_LOUD( "MUTEX %s(%d) %s: %s %s %d", mutex->file, mutex->line, mutex->name, what, file, line );
}

struct CS_Mutex *CS_mutexTakeDetailled( const char *name, const char *file, int line ) {
    struct CS_Mutex *returnValue = privateTakeCSMutex();
    if( returnValue == NULL ) {
        CS_LOG_ERROR("MUTEX %s(%d) CS_Mutex failed to alloc", file, line );
        return NULL;
    }
    returnValue->mutex = privateTakeMutex();
    if( returnValue->mutex == NULL ) {
        CS_LOG_ERROR("MUTEX %s(%d) pthread_mutex_t failed to alloc", file, line );
        privateReturnCSMutex( returnValue );
        return NULL;
    }
    if( name ) {
        snprintf( returnValue->name, MUTEX_MAX_NAME, "%p:%s", returnValue, name);
    } else {
        snprintf( returnValue->name, MUTEX_MAX_NAME, "%p MUTEX", returnValue );
    }
    returnValue->name[ MUTEX_MAX_NAME - 1 ] = 0;
    returnValue->file = file;
    returnValue->line = line;

    printMutex(returnValue,"Taken", file, line);

    return returnValue;
}

void CS_mutexReturnDetailled(struct CS_Mutex *returnMe, const char *file, int line) {
    if( !returnMe ) return;
    printMutex(returnMe,"Return",file, line);
    CS_LOG_ERROR_IF( returnMe->locked, "MUTEX Returning a locked mutex. %s(%d) %s at %s(%d)", returnMe->file, returnMe->line, returnMe->name, file, line );
    privateReturnMutex( returnMe->mutex );
    privateReturnCSMutex( returnMe );
}

void CS_mutexLockDetailled(struct CS_Mutex *toLock,const char *file, int line) {
    printMutex(toLock, "lock", file, line);
    pthread_mutex_lock( toLock->mutex );
    toLock->locked = true;
}

void CS_mutexUnlockDetailled(struct CS_Mutex *toUnlock,const char *file, int line) {
    toUnlock->locked = false;
    pthread_mutex_unlock( toUnlock->mutex );
    printMutex(toUnlock, "unlock", file, line);
}
