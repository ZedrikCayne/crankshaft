#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "crankshaftstringbuilder.h"
#include "crankshaftslaballoc.h"
#include "crankshaftlogger.h"

bool CS_LOG_ERROR_BOOL = true;
bool CS_LOG_WARN_BOOL = true;
bool CS_LOG_INFO_BOOL = false;
bool CS_LOG_TRACE_BOOL = false;
bool CS_LOG_VERBOSE_BOOL = false;
bool CS_LOG_QUIET_BOOL = false;

static bool loggingInitialized = false;

static FILE *outputFile;

#define BUFF_FOR_PREFIX 512

static bool privateInit( const char *fileName ) {
    if( fileName != NULL ) {
        outputFile = fopen( fileName, "a" );
        if( outputFile == NULL ) {
            goto NUKE_ME;
        }
    } else {
        outputFile = stdout;
    }
    loggingInitialized = true;
    return false;
NUKE_ME:
    if( outputFile != NULL ) {
        if( outputFile != stdout ) fclose( outputFile );
        outputFile = NULL;
    }
    return true;
}

static bool privateKill() {
    loggingInitialized = false;
    if( outputFile != NULL ) {
        fflush( outputFile );
        if( outputFile != stdout )
            fclose( outputFile );
        outputFile = NULL;
    }
    
    return false;
}

bool CS_logInit( const char *fileName ) {
    if( loggingInitialized ) {
        privateKill();
    }
    bool returnValue = privateInit( fileName );
    return returnValue;
}

bool CS_logKill(void) {
    if( !loggingInitialized )
        return false;
    loggingInitialized = false;
    privateKill();
    return false;
}

void CS_log(const char *file, int line, const char *fmt, ... ) {
    if( loggingInitialized ) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf( outputFile, fmt, ap );
        va_end(ap);
        fprintf( outputFile, "\n");
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

