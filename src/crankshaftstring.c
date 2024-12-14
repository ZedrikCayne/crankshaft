#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"

#include "crankshaftstring.h"

char *CS_stringCopy( const char *in ) {
    if( !in ) return NULL;
    int nLen = strlen( in );
    char *returnValue = CS_alloc( nLen + 1 );
    strncpy( returnValue, in, nLen + 1 );
    return returnValue;
}

void CS_stringFree( const char *toFree ) {
    if(toFree)CS_free( (void*)toFree );
}


