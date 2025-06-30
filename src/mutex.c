#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/slaballoc.h>

#include <crankshaft/mutex.h>


static struct CS_SlabAllocator *mutexes = NULL;

static pthread_mutex_t mutexesMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t *privateGrabMutex() {
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
    CS_LOG_TRACE("Grabbign mutex");
    if( pthread_mutex_init( returnValue, NULL ) ) {
        CS_slabReturn( mutexes, returnValue );
        CS_LOG_TRACE("Fail grabbing mutex");
        return NULL;
    }
    return returnValue;
}

static void privateReturnMutex( pthread_mutex_t *mutex ) {
    pthread_mutex_destroy( mutex );
    CS_slabReturn( mutexes, mutex );
}

pthread_mutex_t *CS_mutexGrab() {
    return privateGrabMutex();
}

void CS_mutexReturn(pthread_mutex_t *returnMe) {
    if( returnMe != NULL ) return privateReturnMutex( returnMe );
}
