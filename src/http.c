#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <pthread.h>
#include <errno.h>

#include <crankshaft/tempbuff.h>
#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/mime.h>
#include <crankshaft/http.h>
#include <crankshaft/slaballoc.h>
#include <crankshaft/util.h>
#include <crankshaft/network.h>
#include <crankshaft/ssl.h>
#include <crankshaft/util.h>
#include <crankshaft/compress.h>
#include <stdint.h>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define _CONNECT 0x404f4e00
#define _DELETE  0x44454c00
#define _GET     0x47455400
#define _HEAD    0x48454100
#define _POST    0x504f5d00
#define _PUT     0x50555400
#define _TRACE   0x54524100
#define _PRE     0x50524500
#derine _MASK    0xFFFFFF00
#else
#define _CONNECT 0x00434340
#define _DELETE  0x004c4544
#define _GET     0x00544547
#define _HEAD    0x00414548
#define _POST    0x00534f50
#define _PUT     0x00545550
#define _TRACE   0x00415254
#define _PRE     0x00455250
#define _MASK    0x00FFFFFF
#endif

static void *requestSlabAlloc = NULL;
static pthread_mutex_t slabAllocMutex = PTHREAD_MUTEX_INITIALIZER;

static struct CS_RequestReply *privateGetReply() {
    CS_PMUTEX_PROTECT_GLOBAL( requestSlabAlloc, &slabAllocMutex ) {
        requestSlabAlloc = CS_slabInit( "Request Reply Slab", sizeof(struct CS_RequestReply), 100, 8 );
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

static SSL *newSSL( int32_t socket ) {
    SSL *returnValue = CS_sslNew(false);
    if( returnValue ) {
        SSL_set_fd( returnValue, socket );
        if( SSL_connect( returnValue ) <= 0 ) {
            unsigned long err_code = ERR_get_error();
            char err_buf[256];
            ERR_error_string(err_code, err_buf);
            CS_LOG_INFO("Failed negotiate SSL. %s", err_buf);
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

#define SET_STRING(_WHICH) CS_stringInitReferenceCstring(&(_WHICH),startOfToken,currentPoint-startOfToken)

static int32_t privateParseReply( struct CS_RequestReply *replyToParse ) {
    int32_t sizeOfReply = CS_PP_dataSize( replyToParse->buffer );
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
    int32_t currentState = REPLY_HEADER_NAME;
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
                    currentState = REPLY_HEADER_SEPARATOR;
                    SET_STRING( replyToParse->replyHeaders[replyToParse->numReplyHeaders ].header);
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
                    SET_STRING(replyToParse->replyHeaders[ replyToParse->numReplyHeaders ].values);
                    ++currentPoint;
                    REQUIRE_CHAR_NO_EAT(LF);
                    currentState = REPLY_HEADER_NAME;
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
    int32_t code;
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
    "PRE",
};

int32_t CS_httpStringToMethodEnum( const struct CS_String *methodString ) {
    int32_t command = (*(int32_t*)methodString->data & _MASK);
    switch(command) {
        case _CONNECT:
            return CS_HTTP_METHOD_CONNECT;
            break;
        case _DELETE:
            return CS_HTTP_METHOD_DELETE;
            break;
        case _GET:
            return CS_HTTP_METHOD_GET;
            break;
        case _HEAD:
            return CS_HTTP_METHOD_HEAD;
            break;
        case _POST:
            return CS_HTTP_METHOD_POST;
            break;
        case _PUT:
            return CS_HTTP_METHOD_PUT;
            break;
        case _TRACE:
            return CS_HTTP_METHOD_TRACE;
            break;
        case _PRE:
            return CS_HTTP_METHOD_PRE;
            break;
        default:
            return CS_HTTP_METHOD_UNKNOWN;
    }
}

const char *CS_httpResponseEnumToCstring( int32_t responseEnum ) {
    if( responseEnum < 0 || responseEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) return NULL;
    return codeToString[ responseEnum ].value;
}

int32_t CS_httpResponseEnumToCode( int32_t responseEnum ) {
    if( responseEnum < 0 || responseEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) return -1;
    return codeToString[ responseEnum ].code;
}

const char *CS_httpMethodEnumToCstring( int32_t methodEnum ) {
    if( methodEnum < 0 || methodEnum >= CS_MAX_HTTP_METHODS ) return NULL;
    return methodEnumToName[ methodEnum ];
}

static int32_t comp(const void *a, const void *b) {
    struct CodeToReturnString *as = (struct CodeToReturnString *)a;
    struct CodeToReturnString *bs = (struct CodeToReturnString *)b;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    if( as->code < bs->code ) return -1;
    if( as->code > bs->code ) return 1;
#pragma GCC diagnostic pop
    return 0;
}

int32_t CS_httpResponseCodeToEnum( int32_t code ) {
    struct CodeToReturnString *response;
    response = (struct CodeToReturnString *)bsearch( &code, codeToString, CS_ARRAY_SIZE(codeToString), sizeof(codeToString[0]), comp);

    if( response == NULL ) return CS_RESPONSE_INVALID;

    return codeToString - response;
}

static int32_t hexDigitToInt( const char *u ) {
    if( *u < '0' ) return -1;
    if( *u > 'f' ) return -1;
    if( *u <= '9' ) return (int32_t)( *u - '0' );
    if( *u >= 'a' ) return (int32_t)( *u - 'a' ) + 10;
    if( *u > 'F' ) return -1;
    if( *u >= 'A' ) return (int32_t)( *u - 'A' ) + 10;
    return -1;
}

#define DECODE_INVALID_LENGTH -1
#define ENCODE_INVALID_LENGTH -1
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
static int32_t encodeBool[] = {
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
#define highHexDigit(X) nibbleToHex[((((int32_t)(X))&0xF0)>>4)];
#define lowHexDigit(X) nibbleToHex[(((int32_t)(X))&0x0F)];
static int32_t privateEncode( const char *toEncode, int32_t toEncodeBufferLength, char *encodeTo, int32_t outputBufferLength ) {
    const char *in = toEncode;
    char *out = encodeTo;
    char *end = encodeTo + outputBufferLength;
    
    for( int32_t i = 0; ((toEncodeBufferLength==0)&&*in)||(i < toEncodeBufferLength); ++i ) {
        if( encodeBool[ (int32_t)*in ] ) {
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


static int32_t privateDecode( const char *toDecode, int32_t decodeBufferLength, char *output, int32_t outputLength ) {
    const char *in = toDecode;
    char *out = output;
    char *end = output + outputLength;

    for( int32_t i = 0; (decodeBufferLength == 0 && *in) || (i < decodeBufferLength); ++i ) {
        if( toDecode == output ) {
            end = out + 3;
        }
        if( *in == '%' ) {
            if( (decodeBufferLength > 0) && (i + 2 > decodeBufferLength) ) {
                CS_LOG_ERROR("Not enough bytes to decode a %c", '%');
                return DECODE_INVALID_LENGTH;
            }
            ++in;
            int32_t highByte = hexDigitToInt( in );
            if( highByte < 0 ) {
                CS_LOG_ERROR("Invalid hex digit");
                return DECODE_INVALID_LENGTH;
            }
            ++in;
            int32_t lowByte = hexDigitToInt( in );
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
    if( out < output + outputLength ) {
        *out = 0;
    }
    return out - toDecode;
}

static int32_t privateDecodeInPlace( char *toDecode, int32_t decodeBufferLength ) {
    return privateDecode( (const char *)toDecode, decodeBufferLength, toDecode, decodeBufferLength );
}

bool CS_httpUrlDecodeInPlace( struct CS_String *toDecode ) {
    int32_t length = privateDecodeInPlace( (char*)toDecode->data, toDecode->length );
    if( length == DECODE_INVALID_LENGTH ) {
        return true;
    }
    toDecode->length = length;
    return false;
}

struct CS_String *CS_httpUrlDecodeTemp( const struct CS_String *doDecode ) {
    struct CS_String *returnValue = CS_stringTempCopy( doDecode );
    if( returnValue != NULL ) {
        return CS_httpUrlDecodeInPlace( returnValue )?NULL:returnValue;
    } else {
        CS_LOG_ERROR("CS_httpUrlDecodeTemp: String to decode too long");
    }
    return NULL;
}

struct CS_String *CS_httpUrlEncodeTemp( const struct CS_String *toEncode ) {
    int32_t nLen = toEncode->length;
    struct CS_String *returnValue = CS_stringReserveTemp( nLen * 3 );
    if( returnValue != NULL ) {
        int32_t nLength = privateEncode( toEncode->data, nLen, (char*)returnValue->data, nLen * 3 );
        if( nLength == ENCODE_INVALID_LENGTH ) return NULL;
        returnValue->length = nLength;
    } else {
        CS_LOG_ERROR("CS_httpUrlEncodeTemp: String to encode too long.");
    }
    return returnValue;
}

struct CS_StringBuilder *CS_httpUrlDecode( const struct CS_String *toDecode ) {
    int32_t nLen = toDecode->length;
    struct CS_StringBuilder *appendTo = CS_SB_create( nLen + 5 );
    struct CS_StringBuilder *returnValue = CS_httpUrlDecodeAppend( toDecode, appendTo );
    if( returnValue == NULL ) {
        CS_SB_free( appendTo );
    }
    return appendTo;
}

struct CS_StringBuilder *CS_httpUrlEncode( const struct CS_String *toEncode ) {
    int32_t nLen = privateEncode( toEncode->data, toEncode->length, NULL, 0 );
    struct CS_StringBuilder *appendTo = CS_SB_create( nLen + 5 );
    struct CS_StringBuilder *returnValue = CS_httpUrlEncodeAppend( toEncode, appendTo );
    if( returnValue == NULL ) {
        CS_SB_free(appendTo);
    }
    return NULL;
}

struct CS_StringBuilder *CS_httpUrlDecodeAppend( const struct CS_String *toDecode, struct CS_StringBuilder *appendTo ) {
    int32_t remains = CS_SB_remain( appendTo );
    int32_t needed = privateDecode( toDecode->data, toDecode->length, CS_SB_writePosition( appendTo ), remains );
    if( needed >= remains ) {
        if( CS_SB_expandBy( appendTo, needed ) ) return NULL;
        needed = privateDecode( toDecode->data, toDecode->length, CS_SB_writePosition( appendTo ), remains );
    }
    CS_SB_fakeAppend(appendTo, needed);
    return appendTo;
}

struct CS_StringBuilder *CS_httpUrlEncodeAppend( const struct CS_String *toEncode, struct CS_StringBuilder *appendTo ) {
    int32_t remains = CS_SB_remain( appendTo );
    int32_t needed = privateEncode( toEncode->data, toEncode->length, CS_SB_writePosition( appendTo ), remains );
    if( needed >= remains ) {
        if( CS_SB_expandBy( appendTo, needed ) ) return NULL;
        needed = privateEncode( toEncode->data, toEncode->length, CS_SB_writePosition( appendTo ), CS_SB_remain( appendTo ) );
    }
    CS_SB_fakeAppend(appendTo, needed);
    return appendTo;
}

int32_t CS_httpUrlDecodeBinary( const void *toDecode, int32_t decodeBufferLength, void *output, int32_t outputBufferLength ) {
    return privateDecode( (const char *)toDecode, decodeBufferLength, (char *)output, outputBufferLength );
}
int32_t CS_httpUrlEncodeBinary( const void *toEncode, int32_t encodeBufferLength, void *output, int32_t outputBufferLength ) {
    return privateEncode( (const char *)toEncode, encodeBufferLength, output, outputBufferLength );
}

enum WhatKindOfUri {
    URI_UNKNOWN,
    URI_HTTP,
    URI_TELNET,
    URI_SSL,
};

struct UriPrefixToKind {
    const struct CS_String prefix;
    int32_t prefixLength;
    int32_t whatKindOfUri;
};

static struct UriPrefixToKind uriPrefixToKind[] = {
    { CS_STRING("http"), 4, URI_HTTP },
    { CS_STRING("telnet"), 6, URI_TELNET },
    { CS_STRING("ssl"), 3, URI_SSL },
};

static int32_t matchUriToType( const struct CS_String *uri ) {
    for( int32_t i = 0; i < CS_ARRAY_SIZE(uriPrefixToKind); ++i ) {
        if( CS_stringStrncmp( uri, &uriPrefixToKind[i].prefix, uriPrefixToKind[i].prefixLength ) == 0 ) {
            return uriPrefixToKind[i].whatKindOfUri;
        }
    }
    return URI_UNKNOWN;
}

static int32_t privateParseAddressAndPort( const char *currentPoint, const char *endOfData, struct CS_String *address, int32_t *port, bool *ssl, struct CS_String *rest, bool wantSSL, int32_t defaultSSLPort, int32_t defaultNonSSLPort ) {
    const struct CS_String *portChar = NULL;
    const char *start = currentPoint;
    while( currentPoint < endOfData && !(*currentPoint == ':' || *currentPoint == '/') ) ++currentPoint;
    if( address ) CS_stringInitReferenceCstring(address, start, currentPoint - start);
    if( *currentPoint == ':' ) {
        ++currentPoint;
        const char *portCharStart = currentPoint;
        while( currentPoint < endOfData && (*currentPoint != '/') ) ++currentPoint;
        portChar = CS_stringTempReferenceCstring( portCharStart, currentPoint - portCharStart );
    }

    if( port ) {
        if( portChar != NULL ) {
            *port = CS_stringAtoi( portChar ); 
        } else {
            *port = wantSSL?defaultSSLPort:defaultNonSSLPort;
        }
    }

    if( ssl ) *ssl = wantSSL;

    if( rest ) {
        if( currentPoint >= endOfData ) {
            CS_stringInitReferenceCstring(rest,NULL,0);
        } else {
            CS_stringInitReferenceCstring(rest,currentPoint,endOfData - currentPoint);
        }
    }

    return 0;
}

static int32_t privateParseHttpUri( const struct CS_String *uri, struct CS_String *address, int32_t *port, bool *ssl, struct CS_String *rest ) {
    const char *currentPoint = uri->data;
    const char *endOfData = uri->data + uri->length;
    bool wantSSL = false;

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
    return privateParseAddressAndPort( currentPoint, endOfData, address, port, ssl, rest, wantSSL, 443, 80 );
}

static int32_t privateParseTelnetUri( const struct CS_String *uri, struct CS_String *address, int32_t *port, bool *ssl, struct CS_String *rest ) {
    if( uri == NULL ) return -1;

    const char *currentPoint = uri->data;
    const char *endOfData = uri->data + uri->length;

    REQUIRE_CHAR('t');
    REQUIRE_CHAR('e');
    REQUIRE_CHAR('l');
    REQUIRE_CHAR('n');
    REQUIRE_CHAR('e');
    REQUIRE_CHAR('t');
    REQUIRE_CHAR(':');
    REQUIRE_CHAR('/');
    REQUIRE_CHAR('/');
    return privateParseAddressAndPort( currentPoint, endOfData, address, port, ssl, rest, false, 22, 23 );
}

static int32_t privateParseSslUri( const struct CS_String *uri, struct CS_String *address, int32_t *port, bool *ssl, struct CS_String *rest ) {
    if( uri == NULL ) return -1;

    const char *currentPoint = uri->data;
    const char *endOfData = uri->data + uri->length;

    REQUIRE_CHAR('s');
    REQUIRE_CHAR('s');
    REQUIRE_CHAR('l');
    REQUIRE_CHAR(':');
    REQUIRE_CHAR('/');
    REQUIRE_CHAR('/');
    return privateParseAddressAndPort( currentPoint, endOfData, address, port, ssl, rest, true, 22, 23 );
}

static int32_t privateParseUri( const struct CS_String *uri, struct CS_String *address, int32_t *port, bool *ssl, struct CS_String *rest ) {
    int32_t uriType = matchUriToType( uri );
    switch( uriType ) {
        case URI_HTTP:
            return privateParseHttpUri( uri, address, port, ssl, rest );
            break;
        case URI_TELNET:
            return privateParseTelnetUri( uri, address, port, ssl, rest );
            break;
        case URI_SSL:
            return privateParseSslUri( uri, address, port, ssl, rest );
            break;
    }
    return -1;
}

int32_t CS_httpParseUri( const struct CS_String *uri, struct CS_String *addressOut, int32_t *portOut, bool *sslOut, struct CS_String *restOut ) {
    return privateParseUri(uri,addressOut,portOut,sslOut,restOut);
}

static const struct CS_String *headerHas( struct CS_RequestHeader *headers, int32_t numHeaders, const struct CS_String *which ) {
    for( int32_t i = 0; i < numHeaders; ++i ) {
        if( CS_stringStrcmp( &headers[ i ].header, which ) == 0 ) {
            return &headers[ i ].values;
        }
    }
    return NULL;
}

static const bool alreadySent( const struct CS_String **alreadySent, int32_t numAlreadySent, const struct CS_String *which ) {
    for( int32_t i = 0; i < numAlreadySent; ++i ) {
        if( CS_stringStrcmp( alreadySent[ i ], which ) == 0 ) return true;
    }
    return false;
}

static void privateForceAppendHeader( struct CS_StringBuilder *appendTo, 
                               const struct CS_String *header,
                               const struct CS_String *value ) {
    CS_SB_printf(appendTo, "%.*s: %.*s%c%c", header->length, header->data, value->length, value->data, CR, LF);
}

static void privateAppendHeader( struct CS_StringBuilder *appendTo, 
                          struct CS_RequestHeader *headers,
                          int32_t numHeaders,
                          const struct CS_String *header, 
                          const struct CS_String *defaultValue ) {
    const struct CS_String *userHeaderValue = headerHas( headers, numHeaders, header );
    const struct CS_String *headerValue = userHeaderValue?userHeaderValue:defaultValue;
    privateForceAppendHeader( appendTo, header, headerValue );
}

static void privateAppendOthers( struct CS_StringBuilder *appendTo,
                         struct CS_RequestHeader *headers,
                         int32_t numHeaders,
                         const struct CS_String **headersIHaveAlreadySent,
                         int32_t numHeadersAlreadySent ) {
    for( int32_t i = 0; i < numHeaders; ++i ) {
        if( !alreadySent( headersIHaveAlreadySent, numHeadersAlreadySent, &headers[ i ].header ) ) {
            privateForceAppendHeader( appendTo, &headers[ i ].header, &headers[ i ].values );
        }
    }
}

static const struct CS_String *defaultHeaders[] = {
    &CS_STRING("User-Agent"),
    &CS_STRING("Connection"),
    &CS_STRING("Accept"),
    &CS_STRING("Accept-Encoding"),
};

static const struct CS_String *defaultHeadersValue[] = {
    &CS_STRING("Crankshaft"),
    &CS_STRING("close"),
    &CS_STRING("*/*"),
    &CS_STRING("gzip, deflate"),
};

#define CONTINUE_CHUNK_ERROR -1
#define CONTINUE_CHUNK_DONE   0
#define CONTINUE_CHUNK_MORE   1

//returns 0 if we're done... negative on error, positive on 'read more'
static int32_t privateContinueChunked( struct CS_RequestReply *reply ) {
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
        int32_t initialOffset = currentPoint - CS_PP_startOfData(reply->buffer);
        char *end = CS_PP_endOfData( reply->buffer );
        int32_t accumulator = 0;

        CS_LOG_TRACE("Okay, we're here now....about to read hex digits.");

        while( *currentPoint != CR && currentPoint < end ) {
            accumulator <<= 4;
            int32_t digit = hexDigitToInt( currentPoint ); 
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
        int32_t numBytesToEat = currentPoint - initialPoint;
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
struct CS_RequestReply *CS_httpStartRequest( int32_t methodEnum,
                                            const struct CS_String *uri,
                                            struct CS_RequestHeader *headers,
                                            int32_t numHeaders,
                                            struct CS_QueryParameter *queryParameters,
                                            int32_t numQueryParameters,
                                            struct CS_FormParameters *formParameters,
                                            int32_t numFormParameters,
                                            void *data,
                                            int32_t dataLength,
                                            struct CS_RequestReply *reuse ) {
    const char *address;
    int32_t portNum;
    struct CS_String tempAddress;
    struct CS_String rest;
    bool wantSSL;
    const char *method = CS_httpMethodEnumToCstring( methodEnum );
    struct CS_StringBuilder *formString = NULL;
    struct CS_PushPullBuffer *pp = NULL;

    if( headerHas( headers, numHeaders, &CS_STRING("Content-Length") ) ) {
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
    if( formParameters && numFormParameters > 0 ) {
        const struct CS_String *currentlySet = headerHas( headers, numHeaders, &CS_STRING("Content-Type") );
        if( currentlySet != NULL && CS_stringStrcmp(currentlySet, &CS_STRING("application/x-www-form-urlencoded") ) != 0 ) {
            CS_LOG_ERROR("Form parameters set but user has set a content type other than form-urlencoded.");
            return NULL;
        }
    }
    if( privateParseUri(uri, &tempAddress, &portNum, &wantSSL, &rest) < 0 ) {
        CS_LOG_ERROR("CS_httpMakeRequest() Badly formatted URI %s", uri?CS_stringTempCstring(uri):"NULL");
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
    address = CS_stringTempCstring(&tempAddress);
    struct addrinfo *addrInfos = CS_networkLookupAddress( address, portNum );
    //Lookup already has a log with it.
    if( addrInfos == NULL ) return NULL;

    struct CS_StringBuilder *sb = CS_SB_create( INITIAL_STRING_BUILDER_SIZE );
    if( sb == NULL ) {
        CS_LOG_ERROR("CS_httpMakeRequest() OOM.");
        goto CLEANUP;
    }

    char prefix = '?';
    //In case we've put query parameters on the uri already.
    if( rest.length != 0 && CS_stringStrstr(&rest, &CS_STRING("?")) ) prefix = '&';
            
    if( rest.length == 0 ) rest = CS_STRING("/");

    CS_SB_printf(sb, "%s %s", method, CS_stringTempCstring(&rest) );
    if( queryParameters != NULL ) {
        for( int32_t i = 0; i < numQueryParameters; ++i ) {
            CS_SB_printf(sb, "%c%s=%s",
                    prefix,
                    CS_stringTempCstring(CS_httpUrlEncodeTemp(&queryParameters[i].name)),
                    CS_stringTempCstring(CS_httpUrlEncodeTemp(&queryParameters[i].value)) );
            prefix = '&';
        }
    }
    CS_SB_printf(sb, " %s%c%c", "HTTP/1.1", CR, LF);

    for( int32_t i = 0; i < (CS_ARRAY_SIZE(defaultHeaders)); ++i ) {
        privateAppendHeader( sb, headers, numHeaders, defaultHeaders[ i ], defaultHeadersValue[ i ] );
    }
    privateAppendHeader(sb, headers, numHeaders, &CS_STRING("Host"), &tempAddress );
    privateAppendOthers(sb, headers, numHeaders, defaultHeaders, CS_ARRAY_SIZE(defaultHeaders) );

    if( formParameters != NULL && numFormParameters > 0 ) {
        char *empty = "";
        char *ampersand = "&";
        char *currentSeparator = empty;
        formString = CS_SB_create( INITIAL_STRING_BUILDER_SIZE );
        for( int32_t i = 0; i < numFormParameters; ++i ) {
            CS_SB_printf( formString, "%s%s=%s", currentSeparator, 
                    CS_stringTempCstring(&formParameters[ i ].name),
                    CS_stringTempCstring(CS_httpUrlEncodeTemp( &formParameters[i].value ) ) );
            currentSeparator = ampersand;
        }
        privateAppendHeader( sb, headers, numHeaders, &CS_STRING("Content-Type"), &CS_STRING("application/x-www-form-urlencoded") );
        privateForceAppendHeader( sb, &CS_STRING("Content-Length"), CS_stringTempSnprintf(64,"%d", CS_SB_size( formString ) ) );
        
    }

    if( dataLength > 0 && data != NULL ) {
        privateAppendHeader( sb, headers, numHeaders, &CS_STRING("Content-Type"), &CS_STRING("application/octet-stream") );
        privateForceAppendHeader( sb, &CS_STRING("Content-Length"), CS_stringTempSnprintf(64,"%d", dataLength) );
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
    int32_t connectValue = -1;
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
    CS_networkReleaseAddressInfos( addrInfos );
    addrInfos = NULL;

    returnValue->ssl = NULL;
    if( wantSSL ) {
        returnValue->ssl = newSSL( returnValue->remoteSocket );
        if( returnValue->ssl == NULL ) goto CLEANUP;
    }

    pp = CS_SB_getPushPullBuffer(sb);
    do {
        int32_t numBytesSent = wantSSL?CS_PP_writeToSSL( pp, returnValue->ssl):CS_PP_writeToFile( pp, returnValue->remoteSocket );
        if( numBytesSent <= 0 ) {
            goto CLEANUP;
        }
    } while ( CS_PP_dataSize( pp ) > 0 );
    CS_PP_defaultFree( pp );
    pp = NULL;
    CS_SB_free( sb );
    sb = NULL;

    returnValue->buffer = CS_PP_defaultAlloc( PP_BUFFER_SIZE_FOR_RETURN );

    return returnValue;

CLEANUP:
    if( returnValue && !reuse ) CS_httpCloseRequest( returnValue );
    if( formString ) CS_SB_free(formString);
    if( sb ) CS_SB_free(sb);
    if( pp ) CS_PP_defaultFree(pp);
    
    if( addrInfos ) CS_networkReleaseAddressInfos( addrInfos );
    return NULL;

}

int32_t CS_httpFillReplyFromRemote( struct CS_RequestReply *requestReply ) {
    int32_t numBytesRead = requestReply->ssl?CS_PP_readFromSSL( requestReply->buffer, requestReply->ssl ):CS_PP_readFromFile( requestReply->buffer, requestReply->remoteSocket );

    return numBytesRead;
}

int32_t CS_httpPushBufferToRemote( struct CS_RequestReply *requestReply, struct CS_PushPullBuffer *pp ) {
    int32_t totalSent = 0;
    do {
        int32_t numBytesSent = requestReply->ssl?CS_PP_writeToSSL( pp, requestReply->ssl):CS_PP_writeToFile( pp, requestReply->remoteSocket );
        if( numBytesSent <= 0 ) {
            return numBytesSent; 
        }
        totalSent += numBytesSent;
    } while ( CS_PP_dataSize( pp ) > 0 );
    return totalSent;
}

int32_t CS_httpPushBytesToRemote( struct CS_RequestReply *requestReply, void *data, int32_t dataLength ) {
    struct CS_PushPullBuffer *pp = CS_PP_onStaticBuffer( dataLength, data );
    if( !pp ) return -1;
    int32_t bytesSent = CS_httpPushBufferToRemote( requestReply, pp );
    CS_PP_defaultFree(pp);
    return bytesSent;
}


#define MINIMUM_DECOMPRESSION_BUFFER 16384
static int32_t privateDecompressReply( struct CS_RequestReply *reply ) {
    const struct CS_String *contentEncoding = CS_httpReplyHeader( reply, &CS_STRING("Content-Encoding") );
    if( contentEncoding == NULL ) return 0;
    
    bool isGzip = CS_stringStrstr( contentEncoding, &CS_STRING("gzip") ) != NULL;
    bool isDeflate = CS_stringStrstr( contentEncoding, &CS_STRING("deflate") ) != NULL;
    
    if( !isGzip && !isDeflate ) return 0;
    
    CS_Compress ctx;
    CS_compressInit( &ctx );
    
    int32_t initialSize = CS_PP_dataSize( reply->buffer ) * 2;
    if( initialSize < MINIMUM_DECOMPRESSION_BUFFER ) initialSize = MINIMUM_DECOMPRESSION_BUFFER;
    
    struct CS_PushPullBuffer *decompressed = CS_PP_defaultAlloc( initialSize );
    if( decompressed == NULL ) {
        CS_LOG_ERROR("OOM creating a decompression buffer.");
        CS_compressDestroy( &ctx );
        return -1;
    }
    
    while( CS_PP_dataSize( reply->buffer ) > 0 ) {
        long result;
        if( isGzip ) {
            result = CS_compressGunzip( &ctx, reply->buffer, decompressed );
        } else {
            result = CS_compressInflate( &ctx, reply->buffer, decompressed );
        }
        
        if( result < 0 ) {
            CS_PP_defaultFree( decompressed );
            CS_compressDestroy( &ctx );
            return -1;
        }
        
        if( result == 0 && CS_PP_dataSize( reply->buffer ) > 0 ) {
            // Need more space
            int32_t newSize = decompressed->size * 2;
            struct CS_PushPullBuffer *newDecompressed = CS_PP_defaultAlloc( newSize );
            if( newDecompressed == NULL ) {
                 CS_PP_defaultFree( decompressed );
                 CS_compressDestroy( &ctx );
                 return -1;
            }
            CS_PP_moveBuffer( decompressed, newDecompressed );
            CS_PP_defaultFree( decompressed );
            decompressed = newDecompressed;
        } else if (result == 0) {
            break;
        }
    }
    
    CS_compressDestroy( &ctx );
    //CS_PP_defaultFree( reply->buffer );
    reply->decompressedBuffer = decompressed;
    return 0;
}

#define INITIAL_STRING_BUILDER_SIZE 4096
#define PP_BUFFER_SIZE_FOR_RETURN 16384
struct CS_RequestReply *CS_httpMakeRequest( int32_t methodEnum,
                                            const struct CS_String *uri,
                                            struct CS_RequestHeader *headers,
                                            int32_t numHeaders,
                                            struct CS_QueryParameter *queryParameters,
                                            int32_t numQueryParameters,
                                            struct CS_FormParameters *formParameters,
                                            int32_t numFormParameters,
                                            void *data,
                                            int32_t dataLength,
                                            bool autoDecompress,
                                            struct CS_RequestReply *reuse ) {
    struct CS_RequestReply *returnValue = CS_httpStartRequest( methodEnum, uri, headers, numHeaders, queryParameters, numQueryParameters, formParameters, numFormParameters, data, dataLength, reuse );

    if( returnValue == NULL ) return NULL;

    bool wantSSL = returnValue->ssl != NULL;

    char CRLFBUFF[] = { CR,LF };

    if( data && dataLength > 0 ) {
        struct CS_PushPullBuffer *pp = CS_PP_onStaticBuffer( dataLength, data );
        do {
            int32_t numBytesSent = wantSSL?CS_PP_writeToSSL( pp, returnValue->ssl):CS_PP_writeToFile( pp, returnValue->remoteSocket );
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

    int32_t numBytesRead = CS_httpFillReplyFromRemote( returnValue );

    if( numBytesRead < 0 ) {
        CS_LOG_ERROR("Read from remote failed.");
        goto CLEANUP;
    }

    int32_t numBytesParsed = privateParseReply( returnValue );
    if( numBytesParsed < 0 ) goto CLEANUP;

    CS_PP_write( returnValue->buffer, numBytesParsed );

    const struct CS_String *contentLength = CS_httpReplyHeader( returnValue, &CS_STRING("Content-Length") );
    //If we have any content at all...there's likely to be something out there for us.
    if( contentLength != NULL ) {
        int64_t howMuch = CS_stringAtol(contentLength);
        if( howMuch > CS_PP_dataSize( returnValue->buffer ) ) {
            int32_t readMore = CS_httpFillReplyFromRemote( returnValue );
            if( readMore < 0 ) goto CLEANUP;
        }
    }

    //At this point, we're at the front of 'data' if we are 'transfer encoded' at all...
    //we're going to probably need more and to 'fix' our data so everything in the buffer
    //is actual data.
    const struct CS_String *encodingHeader = CS_httpReplyHeader( returnValue, &CS_STRING("Transfer-Encoding") );
    if( encodingHeader && CS_stringStrstr( encodingHeader, &CS_STRING("chunked") ) ) {
        returnValue->chunked = true;
        returnValue->chunkCRLFStillPresent = false;
        returnValue->chunkedBytesOffset = -CS_PP_dataSize( returnValue->buffer );
        CS_LOG_TRACE("Chunked bytes offset is %d", returnValue->chunkedBytesOffset);
        do {
            int32_t continueValue = privateContinueChunked( returnValue );
            if( continueValue == CONTINUE_CHUNK_MORE ) {
                CS_LOG_TRACE("Need more data.");
                //Need more data.
                numBytesRead = CS_httpFillReplyFromRemote( returnValue );
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

    if( autoDecompress && privateDecompressReply( returnValue ) < 0 ) {
        CS_LOG_ERROR("Decompression failed.");
        goto CLEANUP;
    }

    return returnValue;

CLEANUP:
    if( returnValue && !reuse ) CS_httpCloseRequest( returnValue );

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
    if( toReturn->decompressedBuffer ) CS_PP_defaultFree( toReturn->decompressedBuffer );
    toReturn->decompressedBuffer = 0;
    privateReturnReply(toReturn);
}

const struct CS_String *CS_httpReplyHeader( struct CS_RequestReply *reply, const struct CS_String *header ) {
    for( int32_t i = 0; i < reply->numReplyHeaders; ++i ) {
        if( CS_stringStrstr( header, &reply->replyHeaders[ i ].header) )
            return &reply->replyHeaders[ i ].values;
    }
    return NULL;
}

void CS_httpCleanupReplies() {
    pthread_mutex_lock( &slabAllocMutex );
    if( requestSlabAlloc ) CS_slabFree( requestSlabAlloc );
    requestSlabAlloc = NULL;
    pthread_mutex_unlock( &slabAllocMutex );
}
struct CS_PushPullBuffer *CS_httpGetReplyBuffer( struct CS_RequestReply *reply ) {
    if( !reply ) return NULL;
    if( reply->decompressedBuffer ) return reply->decompressedBuffer;
    return reply->buffer;
}
