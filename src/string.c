#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/param.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/string.h>
#include <stdint.h>

char *CS_cstringCopy( const char *in ) {
    if( !in ) return NULL;
    int32_t nLen = strlen( in );
    char *returnValue = CS_alloc( nLen + 1 );
    strncpy( returnValue, in, nLen + 1 );
    return returnValue;
}

void CS_cstringFree( const char *toFree ) {
    if(toFree)CS_free( (void*)toFree );
}

const struct CS_String *CS_stringInitCopyCstring( struct CS_String *out, const char *in, int32_t length ) {
    int32_t stringLength = length < 1 ? strlen( in ) : length;
    char * copyTo = CS_alloc(stringLength + 1);
    if( copyTo ) {
        memcpy( copyTo, in, stringLength );
        out->data = copyTo;
        out->flags = CS_STRING_FLAG_NOT_ALLOCATED | CS_STRING_FLAG_FREE_DATA;
        out->data = in;
        out->length = stringLength;
    } else {
        return NULL;
    }
    return out;
}

const struct CS_String *CS_stringInitReferenceCstring( struct CS_String *out, const char *in, int32_t length ) {
    int32_t stringLength = length < 1 ? strlen(in) : length;
    out->data = in;
    out->length = stringLength;
    out->flags = CS_STRING_FLAG_NOT_ALLOCATED;
    return out;
}

const struct CS_String *CS_stringCopy( struct CS_String *in ) {
    return CS_stringCopyCstring( in->data, in->length );
}

const struct CS_String *CS_stringCopyCstring( const char *in, int32_t length ) {
    int32_t stringLength = length < 1 ? strlen( in ) : length;
    int32_t allocSize = sizeof(struct CS_String) + stringLength + 1;
    struct CS_String *returnValue = CS_alloc( allocSize );
    if( returnValue ) {
        returnValue->length = stringLength;
        returnValue->flags = 0;
        returnValue->data = (const char *)(returnValue + 1);
        memcpy( (char *)returnValue->data, in, stringLength );
    }
    return returnValue;
}

const struct CS_String *CS_stringReferenceCstring( const char *in, int32_t length ) {
    int32_t stringLength = length < 1 ? strlen( in ) : length;
    int32_t allocSize = sizeof(struct CS_String);
    struct CS_String *returnValue = CS_alloc( allocSize );
    if( returnValue ) {
        returnValue->length = stringLength;
        returnValue->flags = 0;
        returnValue->data = in;
    }
    return returnValue;
}

void CS_stringFree( const struct CS_String *toFree ) {
    if( toFree == NULL ) {
        CS_LOG_ERROR("Trying to free a NULL reference to a CS_String");
        return;
    }
    if( toFree->flags & CS_STRING_FLAG_FREE_DATA ) {
        if( toFree->data ) CS_free( (void*)toFree->data );
    }
}

int32_t CS_stringStrncmp( const struct CS_String *left, const struct CS_String *right, int32_t maxLength ) {
    int32_t diffcmp = 0;
    const char *lC, *rC;
    int32_t maxLen = maxLength;
    if( maxLen < 0 ) maxLen = 0x7FFFFFF;
    if( maxLen > left->length ) maxLen = left->length;
    if( maxLen > right->length ) maxLen = right->length;
    lC = left->data; rC = right->data;
    for( int32_t i = 0; i < maxLen; ++i ) {
        diffcmp = *lC++ - *rC++;
        if( diffcmp != 0 ) return diffcmp;
    }
    //If we got to here... one or the other of the strings was shorter than the other and less than the passed in length
    if( maxLength < 0 || maxLen < maxLength ) {
        if( left->length == right->length ) return 0;
        if( left->length < right->length ) return -1;
        return 1;
    }
    return 0;
}

int32_t CS_stringCstrncmp( const struct CS_String *left, const char *right, int32_t maxLength ) {
    struct CS_String tempCString;
    CS_stringInitReferenceCstring( &tempCString, right, -1 );
    return CS_stringStrncmp( left, &tempCString, maxLength );
}

const char *CS_stringStrstr( const struct CS_String *haystack, const struct CS_String *needle ) {
    if( needle == NULL || haystack == NULL ) {
        CS_LOG_ERROR_IF( needle == NULL, "CS_stringStrstr: needle is NULL" );
        CS_LOG_ERROR_IF( haystack == NULL, "CS_stringStrstr: haystack is NULL" );
        return NULL;
    }
    if( needle->length > haystack->length )  return NULL;
    int32_t needleLength = needle->length;
    const char *hayHead = haystack->data;
    const char *needleHead = needle->data;
    int32_t tries = haystack->length - needleLength + 1;
    for( int32_t i = 0; i < tries; ++i ) {
        int32_t j;
        hayHead = haystack->data + i;
        for( j = 0; j < needleLength; ++j ) {
            if( hayHead[j] != needleHead[j] ) break;
        }
        if( j == needleLength ) return hayHead;
    }
    return NULL;
}
const char *CS_stringCStrstr( const struct CS_String *haystack, const char *needle, int32_t length );
const char *CS_stringStrstrC( const char *haystack, int32_t length, const struct CS_String *needle );

