#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "crankshaftslaballoc.h"
#include "crankshaftlogger.h"

bool CS_LOG_ERROR_BOOL = true;
bool CS_LOG_WARN_BOOL = true;
bool CS_LOG_INFO_BOOL = false;
bool CS_LOG_TRACE_BOOL = false;
bool CS_LOG_VERBOSE_BOOL = false;
bool CS_LOG_QUIET_BOOL = false;

static bool logWriting = false;
static bool logRunning = false;
static bool loggingInitialized = false;
static bool loggingReady = false;
static pthread_mutex_t loggingSystemMutex = PTHREAD_MUTEX_INITIALIZER;
static int _maxLineLength = 0;

static void *logSlabAllocator = NULL;
static pthread_t writingThread;

static int outputFileHandle = 0;
static int numLogs = 0;
static int numSubmitted = 0;

struct LogLine {
    struct LogLine *next;
    int logId;
    time_t logTime;
    pthread_t thread;
    const char *fileName;
    int fileLine;
    int length;
    char buffer[];
};

static pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
struct LogLine *head = NULL;
struct LogLine *tail = NULL;

static void grabPrivateMutex() {
    pthread_mutex_lock( &loggingSystemMutex );
}

static void releasePrivateMutex() {
    pthread_mutex_unlock( &loggingSystemMutex );
}

static struct LogLine *newLogLine(const char *file, int line) {
    struct LogLine *returnValue = CS_takeOne( logSlabAllocator );
    returnValue->fileName = file;
    returnValue->fileLine = line;
    returnValue->thread = pthread_self();
    returnValue->logTime = time(NULL);
    returnValue->next = NULL;
    returnValue->logId = 0;
    return returnValue;
}

void returnLogLine(struct LogLine *toReturn) {
    CS_returnOne( logSlabAllocator, toReturn );
}

static void addLogLine( struct LogLine *line ) {
    pthread_mutex_lock(&queueMutex);
    ++numLogs;
    line->logId = numLogs;
    if( head == NULL ) {
        head = line;
    }
    if( tail == NULL ) {
        tail = line;
    } else {
        tail->next = line;
        tail = line;
    }
    pthread_mutex_unlock(&queueMutex);
}

static struct LogLine *takeLogLine() {
    if( head == NULL ) return NULL;
    pthread_mutex_lock(&queueMutex);
    struct LogLine *returnValue = head;
    if( head != NULL ) {
        head = head->next;
    }
    if( head == NULL )
        tail = NULL;
    pthread_mutex_unlock(&queueMutex);
    return returnValue;
}

#define BUFF_FOR_PREFIX 512
static void *logWritingLoop( void *c ) {
    char buff[ 512 ];
    logRunning = true;
    while( true ) {
        struct LogLine *logline = takeLogLine();
        if( logline == NULL ) {
            if( !logWriting ) {
                break;
            }
            usleep( 0 );
        } else {
            int prevByte = snprintf(buff,BUFF_FOR_PREFIX,"%012ld:%012ld: ", logline->logTime, logline->thread );
            int bytesWritten = write( outputFileHandle, buff, prevByte );
            if( bytesWritten < 0 ) write(STDERR_FILENO, buff, prevByte ); 
            bytesWritten = write( outputFileHandle, logline->buffer, logline->length );
            if( bytesWritten < 0 ) write(STDERR_FILENO, logline->buffer, logline->length );
            returnLogLine( logline );
        }
    }
    logRunning = false;
    pthread_exit(0);
    return NULL;
}

static bool privateInit( const char *fileName, int maxLineLength, int initialBuffer ) {
    logSlabAllocator = CS_initSlabAlloc( "LOG SLAB", maxLineLength + sizeof(struct LogLine), initialBuffer, 4 );
    if( logSlabAllocator == NULL ) return true;
    //-2 because we are going to stick '\n' and '\0' on the end...the latter just for making the string pretty to debug
    _maxLineLength = maxLineLength - 2;
    if( fileName != NULL ) {
        outputFileHandle = open( fileName, O_CREAT | O_WRONLY | O_TRUNC );
        if( outputFileHandle < 0 ) {
            goto NUKE_ME;
        }
    } else {
        outputFileHandle = STDOUT_FILENO;
    }

    logWriting = true;

    int threadCreate = pthread_create( &writingThread, NULL, logWritingLoop, NULL );
    if( threadCreate < 0 ) goto NUKE_ME;
    loggingInitialized = true;
    pthread_detach( writingThread );
    return false;
NUKE_ME:
    if( logSlabAllocator != NULL )  {
        CS_freeSlabAlloc( logSlabAllocator );
        logSlabAllocator = NULL;
    }
    if( outputFileHandle > STDERR_FILENO ) {
        close( outputFileHandle );
        outputFileHandle = 0;
    }
    return true;
}

static bool privateKill() {
    logWriting = false;
    loggingInitialized = false;
    while( logRunning ) {
        sleep(1);
    }
    if( outputFileHandle > STDERR_FILENO ) {
        fsync( outputFileHandle );
        close( outputFileHandle );
        outputFileHandle = 0;
    }
    
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

bool CS_logKill(void) {
    if( !loggingInitialized )
        return false;
    grabPrivateMutex();
    loggingReady = false;
    loggingInitialized = false;
    privateKill();
    releasePrivateMutex();
    return false;
}



void CS_log(const char *file, int line, const char *fmt, ... ) {
    ++numSubmitted;
    if( loggingInitialized ) {
        struct LogLine *logline = newLogLine( file, line );
        va_list ap;
        va_start(ap, fmt);
        int nLen = vsnprintf( logline->buffer, _maxLineLength, fmt, ap );
        va_end(ap);
        if( nLen > _maxLineLength ) nLen = _maxLineLength;
        logline->buffer[ nLen ] = '\n';
        logline->buffer[ nLen + 1 ] = 0;
        logline->length = nLen + 1;
        addLogLine( logline );
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

