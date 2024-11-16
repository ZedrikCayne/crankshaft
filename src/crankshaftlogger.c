#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>

#include "crankshaftslaballoc.h"
#include "crankshaftlogger.h"

bool CS_LOG_ERROR_BOOL = true;
bool CS_LOG_WARN_BOOL = true;
bool CS_LOG_INFO_BOOL = false;
bool CS_LOG_TRACE_BOOL = false;
bool CS_LOG_VERBOSE_BOOL = false;
bool CS_LOG_QUIET_BOOL = false;

static bool loggingInitialized = false;
static bool loggingReady = false;
static pthread_mutex_t loggingSystemMutex = PTHREAD_MUTEX_INITIALIZER;

static void *logSlabAllocator = NULL;
static pthread_t writingThread;

static pthread_mutex_t headMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tailMutex = PTHREAD_MUTEX_INITIALIZER;
struct LogLine *head = NULL;
struct LogLine *tail = NULL;

struct LogLine {
    struct LogLine *next;
    const char *fileName;
    int fileLine;
    char buffer[];
};

static void grabPrivateMutex() {
    pthread_mutex_lock( &loggingSystemMutex );
}

static void releasePrivateMutex() {
    pthread_mutex_unlock( &loggingSystemMutex );
}

static bool privateInit( const char *fileName, int maxLineLength, int initialBuffer ) {
    logSlabAllocator = CS_initSlabAlloc( "LOG SLAB", maxLineLength + sizeof(struct LogLine), initialBuffer, 4 );
    return false;
}

static bool privateKill() {
    return false;
}

bool CS_logInit( const char *fileName, int maxLineLength, int initialBuffer ) {
    grabPrivateMutex();
    if( loggingInitialized ) {
        privateKill();
    }
    bool returnValue = privateInit( fileName, maxLineLength, initialBuffer );
    releasePrivateMutex();
    return returnValue;
}

bool CS_logKill() {
    grabPrivateMutex();
    loggingReady = false;
    loggingInitialized = false;
    privateKill();
    releasePrivateMutex();
    return false;
}

void CS_log(const char *file, int line, const char *fmt, ... ) {
    if( loggingInitialized ) {
    } else {
        va_list ap;
        va_start(ap,fmt);
        vprintf(fmt,ap);
        va_end(ap);
        printf("\n");
    }
}



void CS_logRotate(int maxHistory) {
}

void CS_logFile(char *fileName) {
}

