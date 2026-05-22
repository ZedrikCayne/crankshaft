#ifndef __crankshafthttpdoth__
#define __crankshafthttpdoth__
#include <stdbool.h>
#include <openssl/ssl.h>

#include <crankshaft/pushpull.h>
#include <crankshaft/stringbuilder.h>
#include <crankshaft/string.h>
#include <stdint.h>

/********************************************************************
 *
 * Basic HTTP support for making requests. Server also depends on
 * this for creating replies and responding to requests.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

enum CS_HttpMethods {
    CS_HTTP_METHOD_ANY = -1,
    CS_HTTP_METHOD_CONNECT = 0,
    CS_HTTP_METHOD_DELETE,
    CS_HTTP_METHOD_GET,
    CS_HTTP_METHOD_HEAD,
    CS_HTTP_METHOD_POST,
    CS_HTTP_METHOD_PUT,
    CS_HTTP_METHOD_TRACE,
    CS_HTTP_METHOD_PRE,
    CS_HTTP_METHOD_UNKNOWN,
    CS_MAX_HTTP_METHODS = CS_HTTP_METHOD_UNKNOWN
};

enum CS_HTTPResponseCodes {
    CS_RESPONSE_INVALID,
    CS_RESPONSE_100,
    CS_RESPONSE_101,
    CS_RESPONSE_102,
    CS_RESPONSE_103,
    CS_RESPONSE_200,
    CS_RESPONSE_201,
    CS_RESPONSE_202,
    CS_RESPONSE_203,
    CS_RESPONSE_204,
    CS_RESPONSE_205,
    CS_RESPONSE_206,
    CS_RESPONSE_207,
    CS_RESPONSE_208,
    CS_RESPONSE_226,
    CS_RESPONSE_300,
    CS_RESPONSE_301,
    CS_RESPONSE_302,
    CS_RESPONSE_303,
    CS_RESPONSE_304,
    CS_RESPONSE_305,
    CS_RESPONSE_306,
    CS_RESPONSE_307,
    CS_RESPONSE_308,
    CS_RESPONSE_400,
    CS_RESPONSE_401,
    CS_RESPONSE_402,
    CS_RESPONSE_403,
    CS_RESPONSE_404,
    CS_RESPONSE_405,
    CS_RESPONSE_406,
    CS_RESPONSE_407,
    CS_RESPONSE_408,
    CS_RESPONSE_409,
    CS_RESPONSE_410,
    CS_RESPONSE_411,
    CS_RESPONSE_412,
    CS_RESPONSE_413,
    CS_RESPONSE_414,
    CS_RESPONSE_415,
    CS_RESPONSE_416,
    CS_RESPONSE_417,
    CS_RESPONSE_418,
    CS_RESPONSE_421,
    CS_RESPONSE_422,
    CS_RESPONSE_423,
    CS_RESPONSE_424,
    CS_RESPONSE_425,
    CS_RESPONSE_426,
    CS_RESPONSE_428,
    CS_RESPONSE_429,
    CS_RESPONSE_431,
    CS_RESPONSE_451,
    CS_RESPONSE_500,
    CS_RESPONSE_501,
    CS_RESPONSE_502,
    CS_RESPONSE_503,
    CS_RESPONSE_504,
    CS_RESPONSE_507,
    CS_RESPONSE_508,
    CS_RESPONSE_510,
    CS_RESPONSE_511,
    MAX_NUM_CS_RESPONSE_ENUMS
};

struct CS_RequestHeader {
    struct CS_String header;
    struct CS_String values;
};

struct CS_FormParameters {
    struct CS_String name;
    struct CS_String value;
};

struct CS_QueryParameter {
    struct CS_String name;
    struct CS_String value;
};

#define MAX_REPLY_HEADERS 64
struct CS_RequestReply {
    int32_t remoteSocket;
    SSL *ssl;
    int32_t responseEnum;
    int32_t numReplyHeaders;
    //Chunked handling. chunkedBytesOffset is the # of bytes
    //from the 'read' head where we expect the # of bytes in
    //hex. (If it is negative it should already be in the
    //buffer, if positive it is beyond what we've read already)
    bool chunked;
    bool chunkCRLFStillPresent;
    int32_t chunkedBytesOffset;
    struct CS_RequestHeader replyHeaders[MAX_REPLY_HEADERS];
    struct CS_PushPullBuffer *buffer;
};

#define CS_MAX_REQUEST_LENGTH 4096

int32_t CS_httpStringToMethodEnum( const struct CS_String *methodString );
const char *CS_httpMethodEnumToString( int32_t methodEnum );
const char *CS_httpResponseEnumToString( int32_t responseEnum );
int32_t CS_httpResponseEnumToCode( int32_t responseEnum );
bool CS_httpUrlDecodeInPlace( struct CS_String *toDecode );
struct CS_String *CS_httpUrlDecodeTemp( const struct CS_String *doDecode ); 
struct CS_String *CS_httpUrlEncodeTemp( const struct CS_String *toEncode );
struct CS_StringBuilder *CS_httpUrlDecode( const struct CS_String *toDecode );
struct CS_StringBuilder *CS_httpUrlEncode( const struct CS_String *toEncode );
struct CS_StringBuilder *CS_httpUrlDecodeAppend( const struct CS_String *toDecode, struct CS_StringBuilder *appendTo );
struct CS_StringBuilder *CS_httpUrlEncodeAppend( const struct CS_String *toEncode, struct CS_StringBuilder *appendTo );
int32_t CS_httpUrlDecodeBinary( const void *toDecode, int32_t decodeBufferLength, void *output, int32_t outputBufferLength );
int32_t CS_httpUrlEncodeBinary( const void *toEncode, int32_t encodeBufferLength, void *output, int32_t outputBufferLength );

const struct CS_String *CS_httpReplyHeader( struct CS_RequestReply *reply, const struct CS_String *header );

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
                                            struct CS_RequestReply *reuse );

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
                                            struct CS_RequestReply *reuse );

void CS_httpCloseRequest( struct CS_RequestReply *closeMe );

int32_t CS_httpFillReplyFromRemote( struct CS_RequestReply *requestReply );
int32_t CS_httpPushBufferToRemote( struct CS_RequestReply *requestReply, struct CS_PushPullBuffer *pp );
int32_t CS_httpPushBytesToRemote( struct CS_RequestReply *requestReply, void *data, int32_t dataLength );

void CS_httpCleanupReplies();
#ifdef __cplusplus
}
#endif
#endif
