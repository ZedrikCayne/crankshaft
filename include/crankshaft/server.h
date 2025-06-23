#ifndef __serverdoth__
#define __serverdoth__
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

#include <crankshaft/slaballoc.h>
#include <crankshaft/http.h>

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
    int listenSocket;
    int serverPort;
    pthread_t serverThread;
    bool killMe;
    bool threadRunning;
    const char *keyPath;
    const char *certificatePath;
    const char *defaultFileServingPath;
    const char *defaultFileServingFile;
    int defaultFileServingCacheControlMaxAge;
    struct CS_SlabAllocator *replyStack;
    int routeNumbers[CS_MAX_HTTP_METHODS];
    struct CS_Route *routes[CS_MAX_HTTP_METHODS];
    SSL_CTX *sslctx;
};


#define HEADER_MAX 64
#define HEADER_VALUE_MAX 256
struct CS_ReplyHeader {
    char header[ HEADER_MAX ];
    char value[ HEADER_VALUE_MAX ];
};
#define COOKIE_MAX 64
#define COOKIE_VALUE_MAX 256
struct CS_ReplyCookie {
    bool httpOnly;
    char cookie[ COOKIE_MAX ];
    char value[ COOKIE_VALUE_MAX ];
};

struct CS_QueryParameter {
    const char *name;
    const char *value;
};


#define MAX_REQUEST_HEADERS 64
#define MAX_QUERY_PARAMETERS 64
#define MAX_FORM_PARAMETERS 64
#define MAX_REPLY_COOKIES 64

struct CS_RequestInfo {
    bool valid;
    int numHeaders;
    int numParameters;
    int numFormParameters;
    const char *uri;
    const char *method;
    const char *httpVersion;
    int requestMethodEnum;
    struct CS_RequestHeader headers[ MAX_REQUEST_HEADERS ];
    struct CS_QueryParameter parameters[ MAX_QUERY_PARAMETERS ];
    struct CS_FormParameters formParameters[ MAX_FORM_PARAMETERS ];
};

struct CS_ClientInfo {
    int clientSocket;
    struct CS_WebServer *server;
    struct sockaddr_in clientSocketAddress;
    struct CS_PushPullBuffer *buffer;
    struct CS_PushPullBuffer *output;
    void (*disconnectCallback)(struct CS_ClientInfo *info);
    void *persistentData;
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
    int method;
    int routeType;
    int routeLength;
    const char *route;
    bool (*handler)(struct CS_ClientInfo *);
};

struct CS_Reply {
    int returnStatusEnum;
    int contentTypeEnum;
    int numHeaders;
    int numCookies;
    struct CS_ReplyHeader replyHeaders[MAX_REQUEST_HEADERS];
    struct CS_ReplyCookie setCookie[MAX_REPLY_COOKIES];
    const void *outputBuffer;
    int outputLength;
};

struct CS_WebServer *CS_serverStart(int portNum,
                                    const char *certfile,
                                    const char *keyFile,
                                    const char *selfSignHostname,
                                    const char *fileServingPath,
                                    const char *fileServingFile,
                                    int fileCacheControlTimeInSeconds,
                                    struct CS_Route *routes,
                                    int nRoutes);
bool CS_serverKill(struct CS_WebServer *server);

bool CS_serverDiagnostic200( struct CS_ClientInfo *info );
bool CS_serverFileServer( struct CS_ClientInfo *info );

const char *CS_serverGetRequestFormParameter( struct CS_ClientInfo *info, const char *name );
const char *CS_serverGetRequestHeader( struct CS_ClientInfo *info, const char *header );
const char *CS_serverGetRequestQueryParameter( struct CS_ClientInfo *info, const char *name );
const char *CS_serverGetRequestCookie( struct CS_ClientInfo *info, const char *cookie );
bool CS_serverSetReplyHeader( struct CS_Reply *reply, const char *header, const char *value );
bool CS_serverSetReplyHeaderInt( struct CS_Reply *reply, const char *header, int value );
bool CS_serverSetReplyHeaderIfMissing( struct CS_Reply *reply, const char *header, const char *value );
bool CS_serverSetReplyHeaderIntIfMissing( struct CS_Reply *reply, const char *header, int value );
bool CS_serverSetReplyCookie( struct CS_Reply *reply, const char *cookie, const char *value );

struct CS_Reply *CS_serverCreateReply( struct CS_ClientInfo *info, int responseEnum, int mimeEnum, void *replyBuffer, int replyLength );
void CS_serverReturnReply( struct CS_ClientInfo *info, struct CS_Reply *reply );
bool CS_serverDoReply( struct CS_ClientInfo *info, struct CS_Reply *reply );

int CS_serverFillIncomingBuffer( struct CS_ClientInfo *info );
int CS_serverWriteOutputBuffer( struct CS_ClientInfo *info );

#ifdef __cplusplus
}
#endif
#endif
