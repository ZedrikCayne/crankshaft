#ifndef __crankshaftthreaddoth__
#define __crankshaftthreaddoth__
#include <stdbool.h>

#include <crankshaft/list.h>
#include <stdint.h>

/********************************************************************
 *
 * CS_Thread. Wrapper for a pthread. Opinionated.
 *
 * Threads may not be joined, they are all fire and forget.
 *
 * We keep track of all the running threads and even the stopped
 * ones until we 'return' them home.
 *
 * When you start a new one, provide it with a function that will
 * return 'true' if it would like to move to the next state.
 *
 * When created, it will start off in CS_THREAD_INIT. Will be in this
 * state until the pthread is actually launched.
 *
 * Thread function will be called with CS_THREAD_START once. If the
 * start function returns true, a startup error is assumed to have
 * happened and the thread state will go to CS_THREAD_ERROR and it
 * will stop.
 *
 * CS_THREAD_RUNNING will be called until either the thread thread
 * function returns 'true';
 *
 * If the thread was told externally to stop, or if the main function
 * returned true.. it will be called one more time with the state
 * CS_THREAD_STOP. If it returns true it'll be put into
 * CS_THREAD_ERROR, otherwise the thread will hit CS_THREAD_STOPPED.
 *
 * Threads will remain until reaped or explicitly returned.
 *
 * (Attempting to return a not stopped thread will scream and not
 * actually return it.)
 *
 ********************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

struct CS_Thread;

#define CS_THREAD_NAME_MAX 60
enum {
    CS_THREAD_ERROR = -1,
    CS_THREAD_INIT = 0,
    CS_THREAD_START,
    CS_THREAD_RUNNING,
    CS_THREAD_STOP,
    CS_THREAD_STOPPED
};

struct CS_Thread *CS_threadStart(const char *threadName,
        void *context,
        bool (*cycle)(struct CS_Thread *myThread, int32_t threadState, void *context));
bool CS_threadStop( struct CS_Thread *thread );
bool CS_threadIsRunning( struct CS_Thread *thread );
bool CS_threadReturn( struct CS_Thread *thread );
int32_t CS_threadState( struct CS_Thread *thread );
struct CS_Thread *CS_threadMine();
const char *CS_threadName( struct CS_Thread *thread );

const struct CS_List *CS_threadTrackedThreads(void);

#ifdef __cplusplus
}
#endif
#endif
