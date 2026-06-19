#ifndef __serverdoth__
#define __serverdoth__
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

#include <crankshaft/slaballoc.h>
#include <crankshaft/http.h>
#include <crankshaft/logfile.h>
#include <stdint.h>

/********************************************************************
 *
 * Basic HTTP(s) server. Handles HTTP 1.1.
 *
 * Will self-sign a certificate for https serving if told to do so
 * otherwise will accept ye olde certificates in pem formats.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


struct CS_WebServer {
    int32_t listenSocket;
    int32_t serverPort;
    pthread_t serverThread;
    bool killMe;
    bool threadRunning;
    bool wantSSL;
    bool behindProxy;
    const char *keyPath;
    const char *certificatePath;
    const char *defaultFileServingPath;
    const char *defaultFileServingFile;
    int32_t defaultFileServingCacheControlMaxAge;
    struct CS_SlabAllocator *replyStack;
    int32_t routeNumbers[CS_MAX_HTTP_METHODS];
    struct CS_Route *routes[CS_MAX_HTTP_METHODS];
    struct CS_LogFile *logAccess;
};


#define HEADER_MAX 64
#define HEADER_VALUE_MAX 256
struct CS_ReplyHeader {
    union { struct CS_String64 name64; struct CS_String name; } name;
    union { struct CS_String256 value256; struct CS_String value; } value;
};

#define COOKIE_MAX 64
#define COOKIE_VALUE_MAX 256

enum {
    CS_REPLY_COOKIE_SAMESITE_LAX,
    CS_REPLY_COOKIE_SAMESITE_STRICT,
    CS_REPLY_COOKIE_SAMESITE_NONE
};

struct CS_ReplyCookie {
    bool httpOnly;
    int32_t sameSiteEnum;
    union { struct CS_String64 cookie64; struct CS_String cookie; } cookie;
    union { struct CS_String256 value256; struct CS_String value; } value;
};


#define MAX_REQUEST_HEADERS 64
#define MAX_QUERY_PARAMETERS 64
#define MAX_FORM_PARAMETERS 64
#define MAX_REPLY_COOKIES 16

struct CS_RequestInfo {
    bool valid;
    int32_t numHeaders;
    int32_t numParameters;
    int32_t numFormParameters;
    int32_t headerSize;
    struct CS_String uri;
    struct CS_String method;
    struct CS_String httpVersion;
    int32_t requestMethodEnum;
    struct CS_RequestHeader headers[ MAX_REQUEST_HEADERS ];
    struct CS_QueryParameter parameters[ MAX_QUERY_PARAMETERS ];
    struct CS_FormParameters formParameters[ MAX_FORM_PARAMETERS ];
};

struct CS_ClientInfo {
    int32_t clientSocket;
    struct CS_WebServer *server;
    union {
        struct sockaddr clientSocketAddress;
        char sockaddrbuff[64];
    };
    void *tempLast;
    int32_t lastSize;
    struct CS_PushPullBuffer *buffer;
    struct CS_PushPullBuffer *output;
    void (*disconnectCallback)(struct CS_ClientInfo *info);
    void *persistentData;
    const void *appData;
    SSL *ssl;
    struct CS_RequestInfo requestInfo;
};

enum {
    CS_ROUTE_TYPE_FILTER,
    CS_ROUTE_TYPE_WILDCARD,
    CS_ROUTE_TYPE_PREFIX,
    CS_ROUTE_TYPE_EXACT
};

struct CS_Route {
    int32_t method;
    int32_t routeType;
    const struct CS_String *route;
    bool (*handler)(struct CS_ClientInfo *);
    const void *appData;
};

struct CS_Reply {
    int32_t returnStatusEnum;
    int32_t contentTypeEnum;
    int32_t numHeaders;
    int32_t numCookies;
    struct CS_ReplyHeader replyHeaders[MAX_REQUEST_HEADERS];
    struct CS_ReplyCookie setCookie[MAX_REPLY_COOKIES];
    const void *outputBuffer;
    int32_t outputLength;
};

struct CS_WebServer *CS_serverStart(int32_t portNum,
                                    const char *certfile,
                                    const char *keyFile,
                                    const char *selfSignHostname,
                                    const char *fileServingPath,
                                    const char *fileServingFile,
                                    int32_t fileCacheControlTimeInSeconds,
                                    struct CS_Route *routes,
                                    int32_t nRoutes);

bool CS_serverKill(struct CS_WebServer *server);

bool CS_serverDiagnostic200( struct CS_ClientInfo *info );
bool CS_serverReplyError( struct CS_ClientInfo *info, int32_t responseEnum, const char *details );
bool CS_serverFileServer( struct CS_ClientInfo *info );
bool CS_serverPushFile( const char *fileToOpen, struct CS_ClientInfo *info, int32_t cacheSeconds, struct CS_Reply *useMe );

void CS_serverRemoveRequestHeader( struct CS_ClientInfo *info, const struct CS_String *header );
const struct CS_String *CS_serverGetRequestFormParameter( struct CS_ClientInfo *info, const struct CS_String *name );
const struct CS_String *CS_serverGetRequestHeader( struct CS_ClientInfo *info, const struct CS_String *header );
const struct CS_String *CS_serverGetRequestQueryParameter( struct CS_ClientInfo *info, const struct CS_String *name );
const struct CS_String *CS_serverGetRequestCookie( struct CS_ClientInfo *info, const struct CS_String *cookie );
const struct CS_String *CS_serverGetReplyHeader( struct CS_Reply *reply, const struct CS_String *header );
const struct CS_String *CS_serverGetRequestTempIdAddress( struct CS_ClientInfo *info );
bool CS_serverSetReplyHeader( struct CS_Reply *reply, const struct CS_String *header, const struct CS_String *value );
bool CS_serverSetReplyHeaderInt( struct CS_Reply *reply, const struct CS_String *header, int32_t value );
bool CS_serverSetReplyHeaderIfMissing( struct CS_Reply *reply, const struct CS_String *header, const struct CS_String *value );
bool CS_serverSetReplyHeaderIntIfMissing( struct CS_Reply *reply, const struct CS_String *header, int32_t value );
bool CS_serverSetReplyCookie( struct CS_Reply *reply, const struct CS_String *cookie, const struct CS_String *value, bool httpOnly, int32_t sameSiteEnum );

struct CS_Reply *CS_serverCreateReply( struct CS_ClientInfo *info, int32_t responseEnum, int32_t mimeEnum, const void *replyBuffer, int32_t replyLength );
void CS_serverReturnReply( struct CS_ClientInfo *info, struct CS_Reply *reply );
bool CS_serverDoReply( struct CS_ClientInfo *info, struct CS_Reply *reply );

int32_t CS_serverKillClientSocket( struct CS_ClientInfo *info );
int32_t CS_serverFillIncomingBuffer( struct CS_ClientInfo *info );
int32_t CS_serverWriteOutputBuffer( struct CS_ClientInfo *info );

#ifdef __cplusplus
}
#endif
#endif
