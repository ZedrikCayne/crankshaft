#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <pthread.h>
#include <errno.h>

#include "crankshafttempbuff.h"
#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftmime.h"
#include "crankshafthttp.h"
#include "crankshaftslaballoc.h"

static SSL_CTX *globalClientCTX = NULL;
static pthread_mutex_t sslCTXMutex = PTHREAD_MUTEX_INITIALIZER;
static void *requestSlabAlloc = NULL;
static pthread_mutex_t slabAllocMutex = PTHREAD_MUTEX_INITIALIZER;

static struct CS_RequestReply *privateGetReply() {
    if( requestSlabAlloc == NULL ) {
        pthread_mutex_lock( &slabAllocMutex );
        if( requestSlabAlloc == NULL ) requestSlabAlloc = CS_slabInit( "Request Reply Slab", sizeof(struct CS_RequestReply), 100, 8 );
        pthread_mutex_unlock( &slabAllocMutex );
        if( requestSlabAlloc == NULL ) return NULL;
    }
    struct CS_RequestReply *returnValue =  CS_slabTake(requestSlabAlloc);
    memset(returnValue,0,sizeof(struct CS_RequestReply));
    return returnValue;
}

static void privateReturnReply( struct CS_RequestReply *toReturn ) {
    if( requestSlabAlloc == NULL ) return;
    CS_slabReturn(requestSlabAlloc, toReturn);
}

static bool InitSSL() {
    pthread_mutex_lock(&sslCTXMutex);
    if( globalClientCTX ) {
        pthread_mutex_unlock(&sslCTXMutex);
        return false;
    }
    const SSL_METHOD *method = TLS_client_method();
    globalClientCTX = SSL_CTX_new(method);
    pthread_mutex_unlock(&sslCTXMutex);
    return globalClientCTX == NULL;
}

static void KillSSL() {
    pthread_mutex_lock(&sslCTXMutex);
    if( globalClientCTX ) {
        SSL_CTX_free( globalClientCTX );
        globalClientCTX = NULL;
    }
    pthread_mutex_unlock(&sslCTXMutex);
}

static SSL *newSSL( int socket ) {
    if( !globalClientCTX && InitSSL() ) return NULL;
    SSL *returnValue = SSL_new( globalClientCTX );
    if( returnValue ) {
        SSL_set_fd( returnValue, socket );
        if( SSL_connect( returnValue ) <= 0 ) {
            CS_LOG_ERROR("Failed negotiate SSL.");
            SSL_free( returnValue );
            returnValue = NULL;
        }
    }
    return returnValue;
}

#define LF ((char)10)
#define CR ((char)13)
#define HT ((char)9)
#define SP ((char)32)
#define AMPERSAND '&'
#define QUESTION '?'
#define POUND '#'
#define COLON ':'
#define EQUAL '='
#define SPACE ' '

#define EAT_CRLF() if(*currentPoint==CR){++currentPoint;REQUIRE_CHAR(LF);}
#define REQUIRE_CHAR(X) if( currentPoint<endOfData && *currentPoint==X)++currentPoint;else return -1;
#define REQUIRE_CHAR_NO_EAT(X) if( currentPoint<endOfData && *currentPoint!=X)return -1;
#define REQUIRE_CRLF() REQUIRE_CHAR(CR);REQUIRE_CHAR_NO_EAT(LF)

enum {
    REPLY_HEADER_NAME,
    REPLY_HEADER_SEPARATOR,
    REPLY_HEADER_VALUE,
    REPLY_HEADER_DONE
};

static int privateParseReply( struct CS_RequestReply *replyToParse ) {
    int sizeOfReply = CS_PP_dataSize( replyToParse->buffer );
    char *startOfBuffer = CS_PP_startOfData( replyToParse->buffer );
    char *endOfData = startOfBuffer + sizeOfReply;
    char *currentPoint = startOfBuffer;
    REQUIRE_CHAR('H');
    REQUIRE_CHAR('T');
    REQUIRE_CHAR('T');
    REQUIRE_CHAR('P');
    REQUIRE_CHAR('/');
    REQUIRE_CHAR('1');
    REQUIRE_CHAR('.');
    REQUIRE_CHAR('1');
    REQUIRE_CHAR(' ');
    char *endOfParse;
    replyToParse->responseEnum = strtol( currentPoint, &endOfParse, 10 );
    while( currentPoint < endOfData && *currentPoint != SP ) ++currentPoint;
    if( endOfParse != currentPoint ) return -1;
    ++currentPoint;
    while( currentPoint < endOfData && *currentPoint != CR ) ++currentPoint;
    REQUIRE_CHAR(CR);
    REQUIRE_CHAR(LF);
    char *startOfToken = NULL;
    int currentState = REPLY_HEADER_NAME;
    while( currentPoint < endOfData && currentState != REPLY_HEADER_DONE ) {
        if( startOfToken == NULL ) startOfToken = currentPoint;
        switch( currentState ) {
            case REPLY_HEADER_NAME:
                if( *currentPoint == CR ) {
                    ++currentPoint;
                    REQUIRE_CHAR_NO_EAT(LF);
                    currentState = REPLY_HEADER_DONE;
                    break;
                }
                if( *currentPoint == COLON ) {
                    if( replyToParse->numReplyHeaders > MAX_REPLY_HEADERS )
                        return -1;
                    *currentPoint = 0;
                    currentState = REPLY_HEADER_SEPARATOR;
                    replyToParse->replyHeaders[ replyToParse->numReplyHeaders ].header = startOfToken;
                    startOfToken = NULL;
                    break;
                }
                break;
            case REPLY_HEADER_SEPARATOR:
                if( *currentPoint == SPACE ) {
                    startOfToken = NULL;
                    break;
                }
                currentState = REPLY_HEADER_VALUE;
            case REPLY_HEADER_VALUE:
                if( *currentPoint == CR ) {
                    *currentPoint = 0;
                    ++currentPoint;
                    REQUIRE_CHAR_NO_EAT(LF);
                    currentState = REPLY_HEADER_NAME;
                    replyToParse->replyHeaders[ replyToParse->numReplyHeaders ].values = startOfToken;
                    replyToParse->numReplyHeaders++;
                    startOfToken = NULL;
                }
                break;
            default:
                break;
        }
        ++currentPoint;
    }
    if( currentState != REPLY_HEADER_DONE ) return -1;
        
    return currentPoint - startOfBuffer;
}

struct CodeToReturnString {
    int code;
    const char *value;
};

//Keep this in line with the enum in the header
struct CodeToReturnString codeToString[] = {
    {   0, "Invalid" },
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

static const char *methodEnumToName[] = {
    "CONNECT",
    "DELETE",
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "TRACE",
};

const char *CS_httpResponseEnumToString( int responseEnum ) {
    if( responseEnum < 0 || responseEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) return NULL;
    return codeToString[ responseEnum ].value;
}

int CS_httpResponseEnumToCode( int responseEnum ) {
    if( responseEnum < 0 || responseEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) return -1;
    return codeToString[ responseEnum ].code;
}

const char *CS_httpMethodEnumToString( int methodEnum ) {
    if( methodEnum < 0 || methodEnum >= CS_MAX_HTTP_METHODS ) return NULL;
    return methodEnumToName[ methodEnum ];
}

static int comp(const void *a, const void *b) {
    struct CodeToReturnString *as = (struct CodeToReturnString *)a;
    struct CodeToReturnString *bs = (struct CodeToReturnString *)b;
    if( as->code < bs->code ) return -1;
    if( as->code > bs->code ) return 1;
    return 0;
}

int CS_httpResponseCodeToEnum( int code ) {
    struct CodeToReturnString *response;
    response = (struct CodeToReturnString *)bsearch( &code, codeToString, sizeof(codeToString)/sizeof(codeToString[0]), sizeof(codeToString[0]), comp);

    if( response == NULL ) return CS_RESPONSE_INVALID;

    return codeToString - response;
}

static int hexDigitToInt( const char *u ) {
    if( *u < '0' ) return -1;
    if( *u > 'f' ) return -1;
    if( *u <= '9' ) return (int)( *u - '0' );
    if( *u >= 'a' ) return (int)( *u - 'a' ) + 10;
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
    *out = 0;

    return out - encodeTo;
}


static int privateDecode( const char *toDecode, int decodeBufferLength, char *output, int outputLength ) {
    const char *in = toDecode;
    char *out = output;
    char *end = output + outputLength;

    for( int i = 0; (decodeBufferLength == 0 && *in) || (i < decodeBufferLength); ++i ) {
        if( toDecode == output ) {
            end = out + 3;
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
    return out - toDecode;
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

int privateParseUri( const char *uri, char **address, int *port, bool *ssl, const char **rest ) {
    int len = strlen(uri);
    char *tempUri = CS_tempStringCopy( uri );
    char *currentPoint = tempUri;
    char *endOfData = tempUri + len;
    bool wantSSL = false;
    char *portChar = NULL;

    if( uri == NULL ) return -1;

    REQUIRE_CHAR('h');
    REQUIRE_CHAR('t');
    REQUIRE_CHAR('t');
    REQUIRE_CHAR('p');
    if( *currentPoint == 's' ) {
        wantSSL = true;
        ++currentPoint;
    }
    REQUIRE_CHAR(':');
    REQUIRE_CHAR('/');
    REQUIRE_CHAR('/');
    *address = currentPoint;
    while( currentPoint < endOfData && !(*currentPoint == ':' || *currentPoint == '/') ) ++currentPoint;
    if( *currentPoint == ':' ) {
        *currentPoint = 0;
        ++currentPoint;
        portChar = currentPoint;
        while( currentPoint < endOfData && (*currentPoint != '/') ) ++currentPoint;
        *currentPoint = 0;
    } else if ( *currentPoint == '/' ) {
        *currentPoint = 0;
    }
    if( portChar != NULL ) {
        *port = atoi( portChar ); 
    } else {
        *port = wantSSL?443:80;
    }
    *ssl = wantSSL;
    if( currentPoint >= endOfData ) {
        *rest = NULL;
    } else {
        *rest = uri + (currentPoint - tempUri);
    }
    return 0;
}

struct addrinfo *CS_httpLookupAddress( const char *address, int portNum ) {
    struct addrinfo hints = { 0 };
    char portNumString[64];
    snprintf( portNumString, 64, "%d", portNum );
    hints.ai_flags = AI_PASSIVE|AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addrInfos;
    if( getaddrinfo(address,portNum > 0?portNumString:NULL,&hints,&addrInfos) != 0 ) {
        CS_LOG_WARN("CS_httpLookupAddress() Address lookup fail: %s", address);
        return NULL;
    }
    return addrInfos;
}

void CS_httpReleaseAddressInfos( struct addrinfo *infos ) {
    freeaddrinfo( infos );
}

static const char *headerHas( struct CS_RequestHeader *headers, int numHeaders, const char *which ) {
    for( int i = 0; i < numHeaders; ++i ) {
        if( strcmp( headers[ i ].header, which ) == 0 ) {
            return headers[ i ].values;
        }
    }
    return NULL;
}

static const bool alreadySent( const char **alreadySent, int numAlreadySent, const char *which ) {
    for( int i = 0; i < numAlreadySent; ++i ) {
        if( strcmp( alreadySent[ i ], which ) == 0 ) return true;
    }
    return false;
}

static void privateForceAppendHeader( struct CS_StringBuilder *appendTo, 
                               const char *header,
                               const char *value ) {
    CS_SB_printf(appendTo, "%s: %s%c%c", header, value, CR, LF);
}

static void privateAppendHeader( struct CS_StringBuilder *appendTo, 
                          struct CS_RequestHeader *headers,
                          int numHeaders,
                          const char *header, 
                          const char *defaultValue ) {
    const char *userHeaderValue = headerHas( headers, numHeaders, header );
    const char *headerValue = userHeaderValue?userHeaderValue:defaultValue;
    privateForceAppendHeader( appendTo, header, headerValue );
}

static void privateAppendOthers( struct CS_StringBuilder *appendTo,
                         struct CS_RequestHeader *headers,
                         int numHeaders,
                         const char **headersIHaveAlreadySent,
                         int numHeadersAlreadySent ) {
    for( int i = 0; i < numHeaders; ++i ) {
        if( !alreadySent( headersIHaveAlreadySent, numHeadersAlreadySent, headers[ i ].header ) ) {
            privateForceAppendHeader( appendTo, headers[ i ].header, headers[ i ].values );
        }
    }
}

static const char *defaultHeaders[] = {
    "User-Agent",
    "Connection",
    "Accept",
    "Accept-Encoding",
    "Host"
};

static const char *defaultHeadersValue[] = {
    "Crankshaft",
    "close",
    "*/*",
    "identity",
    NULL
};
#define HOST_INDEX 4

#define CONTINUE_CHUNK_ERROR -1
#define CONTINUE_CHUNK_DONE   0
#define CONTINUE_CHUNK_MORE   1

//returns 0 if we're done... negative on error, positive on 'read more'
static int privateContinueChunked( struct CS_RequestReply *reply ) {
    //chunkedBytesOffset represents the offset from the 'read' head where the
    //# of bytes in the next chunk are encoded. When we enter this function
    //we assume that if we've already gotten it in the buffer, we've already
    //removed the leading CRLF pair.
    while( reply->chunked && reply->chunkedBytesOffset <= 0 ) {
        char *currentPoint = CS_PP_endOfData(reply->buffer) + reply->chunkedBytesOffset;
        char *initialPoint = currentPoint;
        if( currentPoint < CS_PP_startOfData(reply->buffer) ) {
            CS_LOG_ERROR("Point where we think we need to be before is before our start.");
            return CONTINUE_CHUNK_ERROR;
        }
        if( reply->chunkCRLFStillPresent ) {
            if( *currentPoint != CR || *(currentPoint + 1) != LF ) {
                CS_LOG_ERROR("This should be a chunk footer CRLF. Was not.");
                return CONTINUE_CHUNK_ERROR;
            }
            if( CS_PP_removeChunk( reply->buffer, currentPoint - CS_PP_startOfData(reply->buffer), 2 ) ) {
                CS_LOG_VERBOSE("Needed to delete 2 bytes but we ran out of buffer...read more.");
                return CONTINUE_CHUNK_MORE;
            }
            reply->chunkCRLFStillPresent = false;
        }
        int initialOffset = currentPoint - CS_PP_startOfData(reply->buffer);
        char *end = CS_PP_endOfData( reply->buffer );
        int accumulator = 0;

        CS_LOG_TRACE("Okay, we're here now....about to read hex digits.");

        while( *currentPoint != CR && currentPoint < end ) {
            accumulator <<= 4;
            int digit = hexDigitToInt( currentPoint ); 
            if( digit < 0 ) {
                CS_LOG_ERROR("Should be hex digits here...was not.");
                return CONTINUE_CHUNK_ERROR;
            }
            ++currentPoint;
            accumulator += digit;
        }
        CS_LOG_TRACE("Accumulator %d", accumulator);
        CS_LOG_TRACE("We have %d left", CS_PP_bufferRemaining( reply->buffer ) );
        CS_LOG_TRACE("Checking if we're at the end.");
        if( currentPoint == end ) {
            CS_LOG_VERBOSE("We're beyond the end, need more.");
            return CONTINUE_CHUNK_MORE;
        }
        ++currentPoint;
        if( currentPoint == end ) {
            CS_LOG_WARN("Beyond the end again, need more.");
            return CONTINUE_CHUNK_MORE;
        }
        if( *currentPoint != LF ) {
            CS_LOG_ERROR("CR should have been followed by a LF.");
            return CONTINUE_CHUNK_ERROR;
        }
        ++currentPoint;
        int numBytesToEat = currentPoint - initialPoint;
        if( CS_PP_removeChunk( reply->buffer, initialOffset, numBytesToEat ) ) {
            CS_LOG_ERROR("This should not be able to happen...");
            return CONTINUE_CHUNK_MORE;
        }
        reply->chunkedBytesOffset += numBytesToEat + accumulator;
        reply->chunkCRLFStillPresent = true;
        if( accumulator == 0 ) {
            return CONTINUE_CHUNK_DONE;
        }
    }
    return CONTINUE_CHUNK_MORE;
}

#define INITIAL_STRING_BUILDER_SIZE 4096
#define PP_BUFFER_SIZE_FOR_RETURN 16384
struct CS_RequestReply *CS_httpMakeRequest( int methodEnum,
                                            const char *uri,
                                            struct CS_RequestHeader *headers,
                                            int numHeaders,
                                            struct CS_FormParameters *formParameters,
                                            int numFormParameters,
                                            void *data,
                                            int dataLength,
                                            struct CS_RequestReply *reuse ) {
    char address[ 128 ];
    const char *rest;
    int portNum;
    char *tempAddress;
    bool wantSSL;
    const char *method = CS_httpMethodEnumToString( methodEnum );
    struct CS_StringBuilder *formString = NULL;
    struct CS_PushPullBuffer *pp = NULL;

    if( headerHas( headers, numHeaders, "Content-Length" ) ) {
        CS_LOG_ERROR("User has also set a Content-Length.");
        return NULL;
    }
    if( data && formParameters ) {
        CS_LOG_ERROR( "Cannot have both data and form parameters at the same time." );
        return NULL;
    }
    if( method == NULL ) {
        CS_LOG_ERROR("CS_httpMakeRequest() Bad methodEnum %d", methodEnum );
        return NULL;
    }
    if( formParameters ) {
        const char *currentlySet = headerHas( headers, numHeaders, "Content-Type" );
        if( currentlySet != NULL && strcmp(currentlySet, "application/x-www-form-urlencoded" ) != 0 ) {
            CS_LOG_ERROR("Form parameters set but user has set a content type other than form-urlencoded.");
            return NULL;
        }
    }
    if( privateParseUri(uri, &tempAddress, &portNum, &wantSSL, &rest) < 0 ) {
        CS_LOG_ERROR("CS_httpMakeRequest() Badly formatted URI %s", uri?uri:"NULL");
        return NULL;
    }
    
    struct CS_RequestReply *returnValue;
    if( reuse ) {
        returnValue = reuse;
    } else {
        returnValue = privateGetReply();
    }
    if( returnValue == NULL ) {
        CS_LOG_ERROR("CS_httpMakeRequest() OOM getting a reply" );
        return NULL;
    }
    strncpy( address, tempAddress, 128 );
    struct addrinfo *addrInfos = CS_httpLookupAddress( address, portNum );
    //Lookup already has a log with it.
    if( addrInfos == NULL ) return NULL;

    struct CS_StringBuilder *sb = CS_SB_create( INITIAL_STRING_BUILDER_SIZE );
    if( sb == NULL ) {
        CS_LOG_ERROR("CS_httpMakeRequest() OOM.");
        goto CLEANUP;
    }

    if( rest == NULL || rest[0] == 0 ) rest = "/";

    CS_SB_printf(sb, "%s %s %s%c%c", method, rest, "HTTP/1.1", CR, LF);

    defaultHeadersValue[HOST_INDEX] = address;
    for( int i = 0; i < (sizeof(defaultHeaders)/sizeof(defaultHeaders[0])); ++i ) {
        privateAppendHeader( sb, headers, numHeaders, defaultHeaders[ i ], defaultHeadersValue[ i ] );
    }
    privateAppendOthers( sb, headers, numHeaders, defaultHeaders, (sizeof(defaultHeaders)/sizeof(defaultHeaders[0])) );

    if( formParameters != NULL && numFormParameters > 0 ) {
        char *empty = "";
        char *ampersand = "&";
        char *currentSeparator = empty;
        formString = CS_SB_create( INITIAL_STRING_BUILDER_SIZE );
        for( int i = 0; i < numFormParameters; ++i ) {
            CS_SB_printf( formString, "%s%s=%s", currentSeparator, 
                    formParameters[ i ].name,
                    CS_httpUrlEncodeTemp( formParameters[i].value ) );
            currentSeparator = ampersand;
        }
        privateAppendHeader( sb, headers, numHeaders, "Content-Type", "application/x-www-form-urlencoded" );
        privateForceAppendHeader( sb, "Content-Length", CS_tempBuffSnprintf(64,"%d", CS_SB_size( formString ) ) );
        
    }

    if( dataLength > 0 && data != NULL ) {
        privateAppendHeader( sb, headers, numHeaders, "Content-Type", "application/octet-stream" );
        privateForceAppendHeader( sb, "Content-Length", CS_tempBuffSnprintf(64,"%d", dataLength) );
    }

    CS_SB_printf(sb,"%c%c",CR,LF);

    if( formString ) {
        CS_SB_append(sb, formString->buffer);
        CS_SB_free( formString );
        formString = NULL;
        CS_SB_printf(sb,"%c%c",CR,LF);
    }

    //Okay, we're ready to actually open the socket and go.
    struct addrinfo *addrInfoIter = addrInfos;
    int connectValue = -1;
    do {
        returnValue->remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
        if( returnValue->remoteSocket < 0 ) {
            CS_LOG_ERROR("Failed to make an outbound socket.");
            break;
        }

        connectValue = connect( returnValue->remoteSocket, addrInfoIter->ai_addr, addrInfoIter->ai_addrlen );
        if( connectValue != 0 ) {
            CS_LOG_ERROR("Error connecting: %s", strerror(errno) );
            close(returnValue->remoteSocket);
            returnValue->remoteSocket = -1;
            addrInfoIter = addrInfoIter->ai_next;
        }
    } while( connectValue == -1 && addrInfoIter != NULL );

    if( returnValue->remoteSocket < 0 || connectValue != 0 )
        goto CLEANUP;
    CS_httpReleaseAddressInfos( addrInfos );
    addrInfos = NULL;

    returnValue->ssl = NULL;
    if( wantSSL ) {
        returnValue->ssl = newSSL( returnValue->remoteSocket );
        if( returnValue->ssl == NULL ) goto CLEANUP;
    }

    pp = CS_SB_getPushPullBuffer(sb);
    do {
        int numBytesSent = wantSSL?CS_PP_writeToSSL( pp, returnValue->ssl):CS_PP_writeToFile( pp, returnValue->remoteSocket );
        if( numBytesSent <= 0 ) {
            goto CLEANUP;
        }
    } while ( CS_PP_dataSize( pp ) > 0 );
    CS_PP_defaultFree( pp );
    pp = NULL;
    CS_SB_free( sb );
    sb = NULL;

    char CRLFBUFF[] = { CR,LF };

    if( data && dataLength > 0 ) {
        pp = CS_PP_onStaticBuffer( dataLength, data );
        do {
            int numBytesSent = wantSSL?CS_PP_writeToSSL( pp, returnValue->ssl):CS_PP_writeToFile( pp, returnValue->remoteSocket );
            if( numBytesSent <= 0 ) {
                goto CLEANUP;
            }
        } while ( CS_PP_dataSize( pp ) > 0 );
        CS_PP_defaultFree( pp );
        pp = NULL;
        if( wantSSL ) {
            SSL_write( returnValue->ssl, CRLFBUFF, 2 );
        } else {
            write( returnValue->remoteSocket, CRLFBUFF, 2 );
        }
     }

    returnValue->buffer = CS_PP_defaultAlloc( PP_BUFFER_SIZE_FOR_RETURN );

    int numBytesRead = wantSSL?CS_PP_readFromSSL( returnValue->buffer, returnValue->ssl ):CS_PP_readFromFile( returnValue->buffer, returnValue->remoteSocket );

    if( numBytesRead <= 0 ) {
        CS_LOG_ERROR("Read from remote failed.");
        goto CLEANUP;
    }

    int numBytesParsed = privateParseReply( returnValue );
    if( numBytesParsed < 0 ) goto CLEANUP;

    CS_PP_write( returnValue->buffer, numBytesParsed );

    //At this point, we're at the front of 'data' if we are 'transfer encoded' at all...
    //we're going to probably need more and to 'fix' our data so everything in the buffer
    //is actual data.
    const char *encodingHeader = CS_httpReplyHeader( returnValue, "Transfer-Encoding" );
    if( encodingHeader && strstr( "chunked", encodingHeader ) ) {
        returnValue->chunked = true;
        returnValue->chunkCRLFStillPresent = false;
        returnValue->chunkedBytesOffset = -CS_PP_dataSize( returnValue->buffer );
        CS_LOG_TRACE("Chunked bytes offset is %d", returnValue->chunkedBytesOffset);
        do {
            int continueValue = privateContinueChunked( returnValue );
            if( continueValue == CONTINUE_CHUNK_MORE ) {
                CS_LOG_TRACE("Need more data.");
                //Need more data.
                numBytesRead = wantSSL?CS_PP_readFromSSL( returnValue->buffer, returnValue->ssl ):CS_PP_readFromFile( returnValue->buffer, returnValue->remoteSocket );
                if( numBytesRead < 0 ) {
                    CS_LOG_TRACE("Error?");
                    goto CLEANUP;
                }
                CS_LOG_TRACE("Read %d extra", numBytesRead);
                returnValue->chunkedBytesOffset -= numBytesRead;
            } else if( continueValue == CONTINUE_CHUNK_ERROR ) {
                CS_LOG_TRACE("Got a chunk error...");
                goto CLEANUP;
            } else {
                CS_LOG_TRACE("Done!");
                break;
            }
        } while( CS_PP_bufferRemaining( returnValue->buffer ) > 0 );
    }

    return returnValue;

CLEANUP:
    if( returnValue && !reuse ) CS_httpCloseRequest( returnValue );
    if( formString ) CS_SB_free(formString);
    if( sb ) CS_SB_free(sb);
    if( pp ) CS_PP_defaultFree(pp);
    
    if( addrInfos ) CS_httpReleaseAddressInfos( addrInfos );
    return NULL;
}

void CS_httpCloseRequest( struct CS_RequestReply *toReturn ) {
    if( toReturn == NULL ) return;
    if( toReturn->remoteSocket > 0 ) close( toReturn->remoteSocket );
    toReturn->remoteSocket = 0;
    if( toReturn->ssl ) SSL_free( toReturn->ssl );
    toReturn->ssl = 0;
    if( toReturn->buffer ) CS_PP_defaultFree( toReturn->buffer );
    toReturn->buffer = 0;
    privateReturnReply(toReturn);
}

const char *CS_httpReplyHeader( struct CS_RequestReply *reply, const char *header ) {
    for( int i = 0; i < reply->numReplyHeaders; ++i ) {
        if( strstr( header, reply->replyHeaders[ i ].header) )
            return reply->replyHeaders[ i ].values;
    }
    return NULL;
}

bool CS_httpInitSSL() {
    return InitSSL();
}

void CS_httpKillSSL() {
    KillSSL();
}

void CS_httpCleanupReplies() {
    pthread_mutex_lock( &slabAllocMutex );
    if( requestSlabAlloc ) CS_slabFree( requestSlabAlloc );
    requestSlabAlloc = NULL;
    pthread_mutex_unlock( &slabAllocMutex );
}
