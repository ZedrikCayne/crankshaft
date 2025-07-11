#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>

#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/logfile.h>
#include <crankshaft/mutex.h>
#include <crankshaft/stringbuilder.h>
#include <crankshaft/tempbuff.h>

struct CS_LogFile {
    FILE *openLogFile;
    char fileName[PATH_MAX];
    struct CS_Mutex *mutex;
    struct CS_StringBuilder *forOutput;
    size_t currentFileBytes;
    int maxBytes;
    int maxRotates;
    int secondsPerRotate;
    time_t lastRotate;
};

static bool privateRotate(struct CS_LogFile *logfile);
static bool privateClose(struct CS_LogFile *logfile);
static bool privateOpen(struct CS_LogFile *logfile );

static char *fileName(struct CS_LogFile *logfile, int which ) {
    CS_SB_reset( logfile->forOutput );
    CS_SB_append( logfile->forOutput, logfile->fileName );
    if( which ) {
        CS_SB_printf( logfile->forOutput, ".%d", which );
    }
    return CS_SB_buffer( logfile->forOutput );
}

static bool fileExist(struct CS_LogFile *logfile, int which ) {
    struct stat fileStat;
    return stat( fileName(logfile,which), &fileStat ) == 0;
}

static bool fileExistString( const char *filename ) {
    struct stat fileStat;
    return stat( filename, &fileStat ) == 0;
}

static bool privateOpen( struct CS_LogFile *logfile ) {
    if( logfile->openLogFile ) return true;

    if( fileExist( logfile, 0 ) ) {
        if( privateRotate( logfile ) ) return true;
    }

    logfile->currentFileBytes = 0;
    logfile->openLogFile = fopen( logfile->fileName, "w" );
    logfile->lastRotate = time(NULL);
    return logfile->openLogFile == NULL;
}

static bool privateClose( struct CS_LogFile *logfile ) {
    if( !logfile->openLogFile ) return true;
    fclose( logfile->openLogFile );
    logfile->openLogFile = NULL;
    return false;
}

static bool privateRotate( struct CS_LogFile *logfile ) {
    bool reopenFile = false;
    if( logfile->openLogFile ) {
        privateClose(logfile);
        reopenFile = true;
    }

    char *moveTo = CS_tempStringCopy( fileName( logfile, logfile->maxRotates ) );
    if( fileExistString( moveTo ) ) remove( moveTo );
    for( int i = logfile->maxRotates - 1; i >= 0; --i ) {
        char *moveFrom = CS_tempStringCopy( fileName( logfile, i ) );
        if( fileExistString( moveFrom ) ) {
            if( rename( moveFrom, moveTo ) ) return true;
        }
        moveTo=moveFrom;
    }

    if( reopenFile ) return privateOpen(logfile);
    return false;
}
int CS_logfilePrintf(struct CS_LogFile *logfile, const char *format, ...) {
    if( !logfile || !logfile->mutex || !logfile->openLogFile ) return -1;
    char timeOutputBuff[ 42 ] = {0};
    int returnValue = -1;
    CS_mutexLock( logfile->mutex );

    time_t currentTime = time(NULL);

    if( logfile->maxBytes && (logfile->currentFileBytes > logfile->maxBytes) ) {
        if( privateRotate( logfile ) ) goto ERR_UNLOCK;
    }

    if( currentTime > 0 ) { 
        if( logfile->secondsPerRotate ) {
            if( (logfile->lastRotate + (logfile->secondsPerRotate*1000)) < currentTime ) {
                if( privateRotate( logfile ) ) goto ERR_UNLOCK;
            }
        }
    } else {
        goto CONTINUE_WITHOUT_TIME;
    }


    CS_SB_reset( logfile->forOutput );
    struct tm tmStruct;
    if( gmtime_r( &currentTime, &tmStruct ) != &tmStruct ) goto CONTINUE_WITHOUT_TIME;
    if( strftime( timeOutputBuff, 42, "%a, %d %b %Y %T %z", &tmStruct ) == 0 ) goto CONTINUE_WITHOUT_TIME;
    CS_SB_append( logfile->forOutput, timeOutputBuff );
    CS_SB_appendChar( logfile->forOutput, ':' );
    CS_SB_appendChar( logfile->forOutput, ' ' );

CONTINUE_WITHOUT_TIME:

    va_list va;
    va_start( va, format );
    struct CS_StringBuilder *sb = CS_SB_vsnprintf( logfile->forOutput, 0, format, va );
    va_end( va );
    if( sb == NULL ) goto ERR_UNLOCK;

    CS_SB_appendChar( sb, '\n' );

    returnValue = fprintf( logfile->openLogFile, "%s", CS_SB_buffer( logfile->forOutput ) );
    //Likely out of disk space..try rotating out an old log to make a bit of room.
    //Lose the log
    if( returnValue < 0 ) {
        privateRotate( logfile );
    } else {
        logfile->currentFileBytes += returnValue;
    }

ERR_UNLOCK:
    CS_mutexUnlock( logfile->mutex );
    return returnValue;
}
struct CS_LogFile *CS_logfileCreate(const char *filename, int rotates, int maxBytes, int secondsPerRotate) {
    struct CS_LogFile *returnValue = CS_allocZero( sizeof( struct CS_LogFile ) );

    if( !returnValue ) goto ERR;

    strncpy( returnValue->fileName, filename, PATH_MAX );

    returnValue->mutex = CS_mutexTakeNamed("LOGFILE");

    if( !returnValue->mutex ) goto ERR;

    returnValue->maxRotates = rotates;
    returnValue->maxBytes = maxBytes;
    returnValue->secondsPerRotate = secondsPerRotate;
    returnValue->forOutput = CS_SB_create( 2048 );

    if( returnValue->forOutput == NULL ) goto ERR;

    if( privateOpen(returnValue) ) goto ERR;

    return returnValue;
ERR:
    CS_logfileDestroy(returnValue);
    if( returnValue ) CS_free(returnValue);
    return NULL;
}
bool CS_logfileClose(struct CS_LogFile *logfile) {
    if( !logfile || !logfile->mutex ) return true;
    CS_mutexLock( logfile->mutex );
    bool returnValue = privateClose( logfile );
    CS_mutexUnlock( logfile->mutex );
    return returnValue;
}
bool CS_logfileReopen(struct CS_LogFile *logfile) {
    if( !logfile || !logfile->mutex ) return true;
    CS_mutexLock( logfile->mutex );
    bool returnValue = privateOpen( logfile );
    CS_mutexUnlock( logfile->mutex );
    return returnValue;
}
bool CS_logfileRotate(struct CS_LogFile *logfile) {
    if( !logfile || !logfile->mutex ) return true;
    CS_mutexLock( logfile->mutex );
    bool returnValue = privateRotate( logfile );
    CS_mutexUnlock( logfile->mutex );
    return returnValue;
}
bool CS_logfileDestroy( struct CS_LogFile *logfile ) {
    if( !logfile ) return true;
    if( logfile->mutex ) CS_mutexLock( logfile->mutex );
    struct CS_Mutex *toUnlock = logfile->mutex;
    logfile->mutex = NULL;
    privateClose( logfile );
    CS_free( logfile );
    if( logfile->forOutput ) CS_SB_free( logfile->forOutput );
    logfile->forOutput = NULL;
    if( toUnlock ) {
        CS_mutexUnlock( toUnlock );
        CS_mutexReturn( toUnlock );
    }
    return false;
}

bool CS_logfileFlush(struct CS_LogFile *logfile) {
    if( !logfile || !logfile->mutex || !logfile->openLogFile ) return true;
    CS_mutexLock( logfile->mutex );

    if( logfile->openLogFile ) fflush( logfile->openLogFile );

    CS_mutexUnlock( logfile->mutex );
    return false;
}
