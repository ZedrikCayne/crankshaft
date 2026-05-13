#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/util.h>
#include <stdint.h>

void *CS_utilLoadWholeFile( const char *filename, int32_t *outSize ) {
    struct stat fileStat;
    if( stat( filename, &fileStat ) < 0 ) goto ERR;
    void *buffer = CS_alloc( fileStat.st_size );
    if( !buffer ) goto ERR;
    FILE *file = fopen( filename, "rb" );
    if( !file ) goto ERR_BUFF;
    int32_t nBytesRead = fread( buffer, 1, fileStat.st_size, file );
    if( nBytesRead < fileStat.st_size ) goto ERR_FILE;
    fclose( file );
    if( outSize ) *outSize = fileStat.st_size;
    return buffer;
ERR_FILE:
    if( file ) fclose( file );
ERR_BUFF:
    if( buffer ) CS_free(buffer);
ERR:
    return NULL;

}



