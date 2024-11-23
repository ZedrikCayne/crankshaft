#include <stdlib.h>
#include <stdio.h>

#include "crankshafttempbuff.h"
#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftmime.h"
#include "crankshafthttp.h"

struct CodeToReturnString {
    int code;
    const char *value;
};

//Keep this in line with the enum in the header
struct CodeToReturnString codeToString[] = {
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 102, "Processing" },
    { 103, "Early Hints" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 207, "Multi-Status" },
    { 208, "Already Reported" },
    { 226, "IM Used" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 306, "unusued" },
    { 307, "Temporary Redirect" },
    { 308, "Permanent Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Payload Too Large" },
    { 414, "URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 418, "I'm a teapot" },
    { 421, "Misdirected Request" },
    { 422, "Unprocessable Content" },
    { 423, "Locked" },
    { 424, "Failed Dependency" },
    { 425, "Too Early" },
    { 426, "Upgrade Required" },
    { 428, "Precondition Required" },
    { 429, "Too Many Requests" },
    { 431, "Request Header Fields Too Large" },
    { 451, "Unavailable For Legal Reasons" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" }, 
    { 504, "Gateway Timeout" },
    { 507, "Insufficient Storage" },
    { 508, "Loop Detected" },
    { 510, "Not Extended" },
    { 511, "Network Authentication Required" }
};

const char *CS_httpResponseEnumToString( int responseEnum ) {
    if( responseEnum < 0 || responseEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) return NULL;
    return codeToString[ responseEnum ].value;
}

int CS_httpResponseEnumToCode( int responseEnum ) {
    if( responseEnum < 0 || responseEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) return -1;
    return codeToString[ responseEnum ].code;
}

static int hexDigitToInt( const char *u ) {
    if( *u < '0' ) return -1;
    if( *u > 'f' ) return -1;
    if( *u <= '9' ) return (int)( *u - '0' );
    if( *u > 'a' ) return (int)( *u - 'a' ) + 10;
    if( *u > 'F' ) return -1;
    if( *u >= 'A' ) return (int)( *u - 'A' ) + 10;
    return -1;
}

#define DECODE_INVALID_LENGTH -1
#define ENCODE_INVALID_LENGTH -1
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
static int encodeBool[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //00 - 0F 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //10 - 1F 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //20 - 2F ' !"#$%&'()*+,-./'
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,    //30 - 3F '0123456789:;<=>?'
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //40 - 4F '@ABCDEFGHIJKLMNO'
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,    //50 - 5F 'PQRSTUVWXYZ[\]^_'
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    //60 - 6F '`abcdefghijklmno'
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,    //70 - 7F 'pqrstuvwxyz{|}~Z'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //80 - 8F 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //90 - 9F 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //A0 - AF 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //B0 - BF 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //C0 - CF 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //D0 - DF 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //E0 - EF 'XXXXXXXXXXXXXXXX'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    //F0 - FF 'XXXXXXXXXXXXXXXX'
};

char nibbleToHex[] = "0123456789ABCDEF";
#define appendC(X) if( out < end )*out = X; ++out
#define highHexDigit(X) nibbleToHex[((((int)(X))&0xF0)>>4)];
#define lowHexDigit(X) nibbleToHex[(((int)(X))&0x0F)];
static int privateEncode( const char *toEncode, int toEncodeBufferLength, char *encodeTo, int outputBufferLength ) {
    const char *in = toEncode;
    char *out = encodeTo;
    char *end = encodeTo + outputBufferLength;
    
    for( int i = 0; ((toEncodeBufferLength==0)&&*in)||(i < toEncodeBufferLength); ++i ) {
        if( encodeBool[ (int)*in ] ) {
            appendC('%');
            appendC(highHexDigit(*in));
            appendC(lowHexDigit(*in));
        } else {
            appendC(*in);
        }
        ++in;
    }

    return out - encodeTo;
}


static int privateDecode( const char *toDecode, int decodeBufferLength, char *output, int outputLength ) {
    const char *in = toDecode;
    char *out = output;
    char *end = output + outputLength;

    for( int i = 0; (decodeBufferLength == 0 && *in) || (i < decodeBufferLength); ++i ) {
        if( toDecode == output ) {
            end = out + 1;
        }
        if( *in == '%' ) {
            if( (decodeBufferLength > 0) && (i + 2 > decodeBufferLength) ) {
                CS_LOG_ERROR("Not enough bytes to decode a %c", '%');
                return DECODE_INVALID_LENGTH;
            }
            ++in;
            int highByte = hexDigitToInt( in );
            if( highByte < 0 ) {
                CS_LOG_ERROR("Invalid hex digit");
                return DECODE_INVALID_LENGTH;
            }
            ++in;
            int lowByte = hexDigitToInt( in );
            if( lowByte < 0 ) {
                CS_LOG_ERROR("Invalid hex digit");
                return DECODE_INVALID_LENGTH;
            }
            i+=2;
            appendC( (char)((highByte << 4) + lowByte) );
        } else {
            appendC( *in );
        }
        ++in;
    }
    *out = 0;
    return toDecode - out;
}

static int privateDecodeInPlace( char *toDecode, int decodeBufferLength ) {
    return privateDecode( (const char *)toDecode, 0, toDecode, 0 );
}

bool CS_httpUrlDecodeInPlace( char *toDecode ) {
    int length = privateDecodeInPlace( toDecode, 0 );
    return length == DECODE_INVALID_LENGTH;
}


char *CS_httpUrlDecodeTemp( const char *doDecode ) {
    char *returnValue = CS_tempStringCopy( doDecode );
    if( returnValue != NULL ) {
        return CS_httpUrlDecodeInPlace( returnValue )?NULL:returnValue;
    } else {
        CS_LOG_ERROR("CS_httpUrlDecodeTemp: String to decode too long");
    }
    return NULL;
}

char *CS_httpUrlEncodeTemp( const char *toEncode ) {
    int nLen = strlen(toEncode);
    char *returnValue = CS_tempBuff( nLen * 3 );
    if( returnValue != NULL ) {
        int nLength = privateEncode( toEncode, nLen, returnValue, nLen * 3 );
        if( nLength == ENCODE_INVALID_LENGTH ) return NULL;
    } else {
        CS_LOG_ERROR("CS_httpUrlEncodeTemp: String to encode too long.");
    }
    return returnValue;
}

struct CS_StringBuilder *CS_httpUrlDecode( const char *toDecode ) {
    int nLen = strlen(toDecode);
    struct CS_StringBuilder *appendTo = CS_SB_create( nLen + 5 );
    struct CS_StringBuilder *returnValue = CS_httpUrlDecodeAppend( toDecode, appendTo );
    if( returnValue == NULL ) {
        CS_SB_free( appendTo );
    }
    return appendTo;
}

struct CS_StringBuilder *CS_httpUrlEncode( const char *toEncode ) {
    int nLen = privateEncode( toEncode, 0, NULL, 0 );
    struct CS_StringBuilder *appendTo = CS_SB_create( nLen + 5 );
    struct CS_StringBuilder *returnValue = CS_httpUrlEncodeAppend( toEncode, appendTo );
    if( returnValue == NULL ) {
    }
    return NULL;
}

struct CS_StringBuilder *CS_httpUrlDecodeAppend( const char *toDecode, struct CS_StringBuilder *appendTo ) {
    int remains = CS_SB_remain( appendTo );
    int needed = privateDecode( toDecode, 0, CS_SB_writePosition( appendTo ), remains );
    if( needed >= remains ) {
        if( CS_SB_expandBy( appendTo, needed ) ) return NULL;
        needed = privateDecode( toDecode, 0, CS_SB_writePosition( appendTo ), remains );
    }
    CS_SB_fakeAppend(appendTo, needed);
    return appendTo;
}

struct CS_StringBuilder *CS_httpUrlEncodeAppend( const char *toEncode, struct CS_StringBuilder *appendTo ) {
    int remains = CS_SB_remain( appendTo );
    int needed = privateEncode( toEncode, 0, CS_SB_writePosition( appendTo ), remains );
    if( needed >= remains ) {
        if( CS_SB_expandBy( appendTo, needed ) ) return NULL;
        needed = privateEncode( toEncode, 0, CS_SB_writePosition( appendTo ), CS_SB_remain( appendTo ) );
    }
    CS_SB_fakeAppend(appendTo, needed);
    return appendTo;
}

int CS_httpUrlDecodeBinary( const void *toDecode, int decodeBufferLength, void *output, int outputBufferLength ) {
    return privateDecode( (const char *)toDecode, decodeBufferLength, (char *)output, outputBufferLength );
}
int CS_httpUrlEncodeBinary( const void *toEncode, int encodeBufferLength, void *output, int outputBufferLength ) {
    return privateEncode( (const char *)toEncode, encodeBufferLength, output, outputBufferLength );
}
