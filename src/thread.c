#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <crankshaft/mutex.h>
#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/slaballoc.h>
#include <crankshaft/util.h>
#include <crankshaft/list.h>

#include <crankshaft/thread.h>

static struct CS_SlabAllocator *threadSlabs;
static struct CS_List *trackedThreads;
static pthread_mutex_t threadMutex = PTHREAD_MUTEX_INITIALIZER;

#define ACTUAL_THREAD_MAX 64

struct CS_Thread {
    char name[ ACTUAL_THREAD_MAX ];
    pthread_t threadId;
    bool (*cycle)(struct CS_Thread *myThread, int threadState, void *context );
    int threadStateEnum;
    bool killThread;
    struct CS_Mutex *mutex;
    void *context;
};

static struct CS_Thread *privateGetThread() {
    CS_PMUTEX_PROTECT_GLOBAL( threadSlabs, &threadMutex ) {
        trackedThreads = CS_listCreate( 64 );
        threadSlabs = CS_slabInit("THREADSLABS", sizeof( struct CS_Thread ), 64, sizeof( void * ) );
        pthread_mutex_unlock(&threadMutex);
        if( threadSlabs == NULL ) return NULL;
    }
    struct CS_Thread *returnValue = CS_slabTakeZero( threadSlabs );
    returnValue->mutex = CS_mutexTake();
    return returnValue;

}

static bool privateReturnThread( struct CS_Thread *thread ) {
    if( !thread ) return true;
    if( thread->mutex ) CS_mutexReturn(thread->mutex);
    thread->mutex = NULL;
    return CS_slabReturn( threadSlabs, thread );
}

static void *threadDriver( void *context ) {
    struct CS_Thread *thread = (struct CS_Thread *)context;
    thread->threadStateEnum = CS_THREAD_START;
    if( thread->cycle( thread, thread->threadStateEnum, thread->context ) ) {
        //Returning true on start means error.
        thread->threadStateEnum = CS_THREAD_ERROR;
        pthread_exit(NULL);
        return NULL;
    }
    thread->threadStateEnum = CS_THREAD_RUNNING;
    while( true ) {
        if( thread->killThread || thread->cycle( thread, thread->threadStateEnum, thread->context ) ) {
            thread->threadStateEnum = CS_THREAD_STOP;
            break;
        }
    }
    if( thread->cycle( thread, thread->threadStateEnum, thread->context ) ) {
        thread->threadStateEnum = CS_THREAD_ERROR;
    }
    thread->threadStateEnum = CS_THREAD_STOPPED;
    pthread_exit(NULL);
    return NULL;
}

struct CS_Thread *CS_threadStart(const char *threadName,
        void *context,
        bool (*cycle)(struct CS_Thread *myThread, int threadState, void *context)) {
    struct CS_Thread *returnValue = privateGetThread();

    pthread_attr_t threadAttr;
    pthread_attr_init( &threadAttr );
    pthread_attr_setstacksize(&threadAttr, PTHREAD_STACK_MIN * 2 );

    strlcpy( returnValue->name, threadName, CS_THREAD_NAME_MAX );
    returnValue->cycle = cycle;
    returnValue->context = context;
    returnValue->threadStateEnum = CS_THREAD_INIT;
    if( pthread_create( &returnValue->threadId, &threadAttr, threadDriver, returnValue ) < 0 ) {
        privateReturnThread( returnValue );
        return NULL;
    }
    pthread_detach( returnValue->threadId );

    pthread_mutex_lock( &threadMutex );
    CS_listPushHead( trackedThreads, returnValue, 0 );
    pthread_mutex_unlock( &threadMutex );

    return returnValue;
}

bool CS_threadStop( struct CS_Thread *thread ) {
    thread->killThread = true;
    return false;
}

bool CS_threadIsRunning( struct CS_Thread *thread ) {
    return thread->threadStateEnum >= CS_THREAD_INIT && thread->threadStateEnum < CS_THREAD_STOP;
}

bool CS_threadReturn( struct CS_Thread *thread ) {
    return false;
}

int CS_threadState( struct CS_Thread *thread ) {
    return thread->threadStateEnum;
}

struct CS_Thread *CS_threadMine() {
    return NULL;
}

const struct CS_List *CS_threadTrackedThreads(void) {
    return NULL;
}

const char *CS_threadName( struct CS_Thread *thread ) {
    if( !thread ) return "NULL";
    return thread->name;
}


