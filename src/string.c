#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <sys/param.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/linearalloc.h>

#include <crankshaft/string.h>
#include <stdint.h>

struct CS_String *CS_stringCopyCstring( const char *in, int32_t length ) {
    int32_t stringLength = length < 0 ? strlen( in ) : length;
    int32_t allocSize = sizeof(struct CS_String) + stringLength + 1;
    struct CS_String *returnValue = CS_alloc( allocSize );
    if( returnValue ) {
        returnValue->length = stringLength;
        returnValue->flags = CS_STRING_FLAG_ALLOCATED;
        returnValue->data = (const char *)(returnValue + 1);
        if( in ) {
            memcpy( (char *)returnValue->data, in, stringLength );
        } else {
            memset( (char *)returnValue->data, 0, stringLength );
        }
    }
    return returnValue;
}

struct CS_String *CS_stringReserveTemp( int32_t length ) {
    return CS_stringTempCopyCstring( NULL, length );
}

struct CS_String *CS_stringInitLinearCopyCstring( struct CS_String *out, const char *in, int32_t length, struct CS_LinearAllocator *allocator ) {
    int32_t stringLength = length < 0 ? strlen( in ) : length;
    int32_t allocSize = stringLength + 1;
    char *destString = CS_linearTake(allocator, allocSize, sizeof(void*));
    memcpy(destString,in,stringLength);
    destString[stringLength] = 0;
    return (struct CS_String*)CS_stringInitReferenceCstring(out, destString, stringLength);
}

struct CS_String *CS_stringLinearCopyCstring( const char *in, int32_t length, struct CS_LinearAllocator *allocator ) {
    int32_t stringLength = length < 0 ? strlen( in ) : length;
    int32_t allocSize = sizeof(struct CS_String) + stringLength + 1;
    struct CS_String *returnValue = CS_linearTake( allocator, allocSize, sizeof(void*) );
    if( returnValue ) CS_stringInitCopyCstring(returnValue, in, stringLength);

    return returnValue;
}

struct CS_String *CS_stringCopyToStatic( struct CS_String *out, const struct CS_String *in, int32_t staticSize ) {
    if( out == NULL ) {
        CS_LOG_ERROR("CS_stringCopyToStatic: NULL output variable.");
        return NULL;
    }
    if( in == NULL ) {
        CS_LOG_ERROR("CS_stringCopyToStatic: NULL input variable.");
        return NULL;
    }
    if( in->length > staticSize ) {
        CS_LOG_ERROR("CS_stringCopyToStatic: input length bigger than the static buffer.");
        return NULL;
    }
    struct CS_String16 *staticStandIn = (struct CS_String16 *)out;
    staticStandIn->data = staticStandIn->realData;
    staticStandIn->flags = staticSize << 4;
    staticStandIn->length = in->length;
    memcpy( staticStandIn->realData, in->data, in->length );
    return out;
}
struct CS_String *CS_stringCopyCstringToStatic( struct CS_String *out, int32_t staticSize, const char *in, int32_t inLength ) {
    struct CS_String temp;
    int32_t realLength = (inLength<0)?strlen(in):inLength;
    temp.length = realLength;
    temp.data = in;
    CS_stringCopyToStatic(out, &temp, staticSize);
    return out;
}

struct CS_String *CS_stringTempCopyCstring( const char *in, int32_t length ) {
    int32_t stringLength = length < 0 ? strlen( in ) : length;
    int32_t allocSize = sizeof(struct CS_String) + stringLength;
    struct CS_String *returnValue = (struct CS_String *)CS_tempBuff( allocSize );
    if( returnValue ) {
        returnValue->length = stringLength;
        returnValue->flags = 0;
        returnValue->data = (const char *)(returnValue + 1);
        if( in != NULL )
            memcpy( (char *)returnValue->data, in, stringLength );
        else
            memset( (char *)returnValue->data, 0, stringLength );
    }
    return returnValue;
}

struct CS_String *CS_stringTempCopy( const struct CS_String *in ) {
    return CS_stringTempCopyCstring( in->data, in->length );
}

struct CS_String *CS_stringInitCopyCstring( struct CS_String *out, const char *in, int32_t length ) {
    int32_t stringLength = length < 0 ? strlen( in ) : length;
    char * copyTo = CS_alloc(stringLength);
    if( copyTo ) {
        memcpy( copyTo, in, stringLength );
        out->data = copyTo;
        out->flags = CS_STRING_FLAG_FREE_DATA;
        out->length = stringLength;
    } else {
        return NULL;
    }
    return out;
}

struct CS_String *CS_stringInitCopy( struct CS_String *out, const struct CS_String *in ) {
    return CS_stringInitCopyCstring( out, in->data, in->length );
}

const struct CS_String *CS_stringInitReference( struct CS_String *out, const struct CS_String *in ) {
    if( out == NULL ) return NULL;
    if( in == NULL ) return NULL;
    out->length = in->length;
    out->flags = 0;
    out->data = in->data;
    return out;
}

const struct CS_String *CS_stringInitReferenceCstring( struct CS_String *out, const char *in, int32_t length ) {
    int32_t stringLength = length < 0 ? strlen(in) : length;
    out->data = in;
    out->length = stringLength;
    out->flags = 0;
    return out;
}

const struct CS_String *CS_stringTempReferenceCstring( const char *in, int32_t length ) {
    struct CS_String *returnValue = CS_tempBuff( sizeof(struct CS_String) );
    if( returnValue ) {
        CS_stringInitReferenceCstring( returnValue, in, length );
    }
    return returnValue;
}

struct CS_String *CS_stringCopy( const struct CS_String *in ) {
    return CS_stringCopyCstring( in->data, in->length );
}

const struct CS_String *CS_stringReferenceCstring( const char *in, int32_t length ) {
    int32_t stringLength = length < 0 ? strlen( in ) : length;
    int32_t allocSize = sizeof(struct CS_String);
    struct CS_String *returnValue = CS_alloc( allocSize );
    if( returnValue ) {
        returnValue->length = stringLength;
        returnValue->flags = CS_STRING_FLAG_ALLOCATED;
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
    if( toFree->flags & CS_STRING_FLAG_ALLOCATED ) {
        CS_free((void*)toFree);
    }
}

int32_t CS_stringStrcmp( const struct CS_String *left, const struct CS_String *right ) {
    return CS_stringStrncmp( left, right, CS_STRING_CMP_WHOLE );
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

int32_t CS_stringStrcasecmp( const struct CS_String *left, const struct CS_String *right ) {
    return CS_stringStrncasecmp(left,right,CS_STRING_CMP_WHOLE);
}

int32_t CS_stringStrncasecmp( const struct CS_String *left, const struct CS_String *right, int32_t maxLength ) {
    int32_t diffcmp = 0;
    const char *lC, *rC;
    int32_t maxLen = maxLength;
    if( maxLen < 0 ) maxLen = 0x7FFFFFF;
    if( maxLen > left->length ) maxLen = left->length;
    if( maxLen > right->length ) maxLen = right->length;
    lC = left->data; rC = right->data;
    for( int32_t i = 0; i < maxLen; ++i ) {
        diffcmp = *lC - *rC;
        if( diffcmp != 0 ) {
            if( *lC < 'a' || *lC > 'z' || *rC < 'a' || *rC > 'z' ) return diffcmp;
            char leftCap = (*lC >= 'a' || *lC <= 'z')?(*lC)-32:*lC;
            char rightCap = (*rC >= 'a' || *rC <= 'z')?(*rC)-32:*rC;
            diffcmp = leftCap - rightCap;
            if( diffcmp != 0 ) return diffcmp;
        }
    }
    //If we got to here... one or the other of the strings was shorter than the other and less than the passed in length
    if( maxLength < 0 || maxLen < maxLength ) {
        if( left->length == right->length ) return 0;
        if( left->length < right->length ) return -1;
        return 1;
    }
    return 0;
}

static const struct CS_String *_stringStrstr( const struct CS_String *haystack, const struct CS_String *needle, bool temp ) {
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
        if( j == needleLength ) return temp?CS_stringTempReferenceCstring(hayHead, haystack->length - i):CS_stringReferenceCstring( hayHead, haystack->length - i );
    }
    return NULL;
}

const struct CS_String *CS_stringStrstr( const struct CS_String *haystack, const struct CS_String *needle ) {
    return _stringStrstr( haystack, needle, false );
}

const struct CS_String *CS_stringTempStrstr( const struct CS_String *haystack, const struct CS_String *needle ) {
    return _stringStrstr( haystack, needle, true );
}

static const struct CS_String *_stringStrrstr( const struct CS_String *haystack, const struct CS_String *needle, bool temp ) {
    if( needle == NULL || haystack == NULL ) {
        CS_LOG_ERROR_IF( needle == NULL, "CS_stringStrrstr: needle is NULL" );
        CS_LOG_ERROR_IF( haystack == NULL, "CS_stringStrrstr: haystack is NULL" );
        return NULL;
    }
    if( needle->length > haystack->length )  return NULL;
    int32_t needleLength = needle->length;
    const char *hayHead = haystack->data;
    const char *needleHead = needle->data;
    int32_t tries = haystack->length - needleLength + 1;
    for( int32_t i = tries; i >= 0; --i ) {
        int32_t j;
        hayHead = haystack->data + i;
        for( j = 0; j < needleLength; ++j ) {
            if( hayHead[j] != needleHead[j] ) break;
        }
        if( j == needleLength ) {
            return temp?CS_stringTempReferenceCstring( hayHead, haystack->length - i ):CS_stringReferenceCstring( hayHead, haystack->length - i );
        }
    }
    return NULL;
}


const struct CS_String *CS_stringStrrstr( const struct CS_String *haystack, const struct CS_String *needle ) {
    return _stringStrrstr(haystack, needle, false );
}

const struct CS_String *CS_stringTempStrrstr( const struct CS_String *haystack, const struct CS_String *needle ) {
    return _stringStrrstr(haystack, needle, true );
}

int64_t CS_stringAtol( const struct CS_String *toAtol ) {
    int64_t accumulator = 0;
    int64_t startChar = 0;
    bool isNegative = false;
    const char *readHead = toAtol->data;
    if( *readHead == '+' || *readHead == '-' ) {
        isNegative = *readHead == '-';
        startChar++;
        ++readHead;
    }
    for( int i = startChar; i < toAtol->length; ++i ) {
        if( *readHead < '0' || *readHead > '9' ) break;
        accumulator = ((accumulator * 10) + (*readHead - '0'));
        ++readHead;
    }
    return isNegative?-accumulator:accumulator;
}

int32_t CS_stringAtoi( const struct CS_String *toAtoi ) {
    int64_t result = CS_stringAtol( toAtoi );
    if( result > INT32_MAX || result < INT32_MIN ) return 0;
    return result;
}

static const void copyTo( char *dest, const struct CS_String *from ) {
    memcpy( dest, from->data, from->length );
    dest[from->length] = 0;
}

const char *CS_stringCstring( const struct CS_String *from ) {
    char *dest = CS_alloc(from->length + 1);
    if( dest ) copyTo(dest,from);
    return dest;
}

const char *CS_stringTempCstring( const struct CS_String *from ) {
    char *dest = CS_tempBuff(from->length + 1);
    if( dest ) copyTo(dest,from);
    return dest;
}

const char *CS_stringTempCstringOrNULL( const struct CS_String *from ) {
    if( from == NULL ) return "NULL";
    return CS_stringTempCstring(from);
}

const struct CS_String *CS_stringTempSnprintf(int32_t max, const char *fmt, ...) {
    struct CS_String *returnValue = CS_tempBuff(max + sizeof(struct CS_String));
    if( returnValue ) {
        char *tBuff = (char*)(returnValue + 1);
        returnValue->data = tBuff;
        va_list ap;
        va_start( ap, fmt );
        int32_t endy = vsnprintf( (char*)tBuff, max, fmt, ap );
        va_end( ap );
        if( endy > max ) returnValue->length = max;
        else returnValue->length = endy;
    }
    return returnValue;
}

const struct CS_String *CS_stringSlice( const struct CS_String *source, int32_t startIndex, int32_t lengthOrNegative ) {
    if( source == NULL ) return NULL;
    if( startIndex > source->length ) return NULL;
    int32_t lengthToPull = lengthOrNegative;
    if( lengthToPull < 0 ) lengthToPull = source->length - startIndex;
    if( lengthToPull + startIndex > source->length ) return NULL;
    return CS_stringCopyCstring( source->data + startIndex, lengthToPull );
}

const struct CS_String *CS_stringSliceTemp( const struct CS_String *source, int32_t startIndex, int32_t lengthOrNegative ) {
    if( source == NULL ) return NULL;
    if( startIndex > source->length ) return NULL;
    int32_t lengthToPull = lengthOrNegative;
    if( lengthToPull < 0 ) lengthToPull = source->length - startIndex;
    if( lengthToPull + startIndex > source->length ) return NULL;
    return CS_stringTempCopyCstring( source->data + startIndex, lengthToPull );
}

const struct CS_String *CS_stringSliceReference( const struct CS_String *source, int32_t startIndex, int32_t lengthOrNegative ) {
    if( source == NULL ) return NULL;
    if( startIndex > source->length ) return NULL;
    if( lengthOrNegative + startIndex > source->length ) return NULL;
    return CS_stringTempCopyCstring( source->data + startIndex, lengthOrNegative );
}

const struct CS_String *CS_stringSliceTempReference( const struct CS_String *source, int32_t startIndex, int32_t lengthOrNegative ) {
    if( source == NULL ) return NULL;
    if( startIndex > source->length ) return NULL;
    if( lengthOrNegative + startIndex > source->length ) return NULL;
    return CS_stringTempReferenceCstring( source->data + startIndex, lengthOrNegative );
}

static bool matchDelimeter( const struct CS_String *delimeters, const char *this ) {
    const char *deliHead = delimeters->data;
    const char *deliEnd = deliHead + delimeters->length;

    while( deliHead < deliEnd ) {
        if( *deliHead == *this )
            return true;
        ++deliHead;
    }
    return false;
}

struct CS_String *CS_stringTempStrtok( const struct CS_String *source, const struct CS_String *delimeters, const char **savePtr ) {
    if( savePtr == NULL ) return NULL;
    if( source == NULL ) return NULL;
    if( delimeters == NULL ) return NULL;
    const char *returnStart = NULL;
    const char *currentReadHead = *savePtr;
    const char *endOfString = source->data + source->length;
    if( currentReadHead >= endOfString ) return NULL;

    if( currentReadHead == NULL ) currentReadHead = source->data;

    //The current saved head points at the last delimited. Or we're at the start of the string....as per regular strtok we need to skip until we find the 'first' non delimeter.
    while( currentReadHead < endOfString && matchDelimeter( delimeters, currentReadHead ) ) ++currentReadHead;

    if( currentReadHead >= endOfString ) return NULL;

    returnStart = currentReadHead;

    while( currentReadHead < endOfString && !matchDelimeter( delimeters, currentReadHead ) ) ++currentReadHead;

    *savePtr = currentReadHead;

    return (struct CS_String*)CS_stringTempReferenceCstring( returnStart, currentReadHead - returnStart );
}


struct CS_String *CS_stringTempStrrtok( const struct CS_String *source, const struct CS_String *delimeters, const char **savePtr ) {
    if( savePtr == NULL ) return NULL;
    if( source == NULL ) return NULL;
    if( delimeters == NULL ) return NULL;
    const char *returnEnd = NULL;
    const char *currentReadHead = *savePtr;
    const char *startOfString = source->data;

    const char *endOfString = (source->length == 0)?source->data:source->data + source->length - 1;

    if( currentReadHead == startOfString ) return NULL;

    if( currentReadHead == NULL ) currentReadHead = endOfString;

    //The current saved head points at the last delimited. Or we're at the start of the string....as per regular strtok we need to skip until we find the 'first' non delimeter.
    while( currentReadHead > startOfString && matchDelimeter( delimeters, currentReadHead ) ) --currentReadHead;

    if( currentReadHead == startOfString ) return NULL;

    returnEnd = currentReadHead;

    while( currentReadHead > startOfString && !matchDelimeter( delimeters, currentReadHead ) ) --currentReadHead;

    *savePtr = currentReadHead;

    if( currentReadHead != startOfString ) ++currentReadHead;

    return (struct CS_String*)CS_stringTempReferenceCstring( currentReadHead, returnEnd - currentReadHead + 1 );
}

#define LF ((char)10)
#define CR ((char)13)
#define HT ((char)9)
#define SP ((char)32)
static bool isWhitespace(const char *isIt) {
    switch(*isIt) {
        case LF:
        case HT:
        case SP:
        case CR:
            return true;
        default:
            return false;
    }
}
void CS_stringLtrim( struct CS_String *trimmable ) {
    int32_t i;
    for( i = 0; i < trimmable->length; ++i ) {
        if( !isWhitespace(trimmable->data + i) )
            break;
    }
    trimmable->data += i;
    trimmable->length -= i;
}

void CS_stringRtrim( struct CS_String *trimmable ) {
    int32_t i;
    for( i = 0; i < trimmable->length; ++i ) {
        if( !isWhitespace(trimmable->data +trimmable->length - i - 1) )
            break;
    }
    trimmable->length -= i;
}

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

int32_t CS_stringStrchr( const struct CS_String *haystack, char needle ) {
    for( int32_t i = 0; i < haystack->length; ++i ) {
        if( needle == haystack->data[i] ) return i;
    }
    return -1;
}

int32_t CS_stringStrrchr( const struct CS_String *haystack, char needle ) {
    for( int32_t i = haystack->length; i >= 0; --i ) {
        if( needle == haystack->data[i] ) return i;
    }
    return -1;
}

bool CS_stringOnlyHas( const struct CS_String *toCheck, const struct CS_String *theseCharacters ) {
    for( int32_t i = 0; i < toCheck->length; ++i ) {
        if( CS_stringStrchr(theseCharacters,toCheck->data[i]) < 0 ) return false;
    }
    return true;
}

bool CS_stringAlnum( const struct CS_String *toCheck ) {
    for( int32_t i = 0; i < toCheck->length; ++i ) {
        if( !isalnum(toCheck->data[i]) ) return false;
    }
    return true;
}
