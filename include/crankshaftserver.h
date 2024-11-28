#ifndef __serverdoth__
#define __serverdoth__
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

#include "crankshafthttp.h"

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
    void *replyStack;
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

struct CS_QueryParameter {
    const char *name;
    const char *value;
};


#define MAX_REQUEST_HEADERS 64
#define MAX_QUERY_PARAMETERS 64
#define MAX_FORM_PARAMETERS 64

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
    struct CS_ReplyHeader *replyHeaders;
    const void *outputBuffer;
    int outputLength;
};

struct CS_WebServer *CS_StartWebServer(int portNum,
                                           const char *certfile,
                                           const char *keyFile,
                                           const char *fileServingPath,
                                           const char *fileServingFile,
                                           int fileCacheControlTimeInSeconds,
                                           struct CS_Route *routes,
                                           int nRoutes);
bool CS_KillWebServer(struct CS_WebServer *server);

bool CS_Diagnostic200( struct CS_ClientInfo *info );
bool CS_FileServer( struct CS_ClientInfo *info );

const char *CS_GetFormParameter( struct CS_ClientInfo *info, const char *name );
const char *CS_GetRequestHeader( struct CS_ClientInfo *info, const char *header );
const char *CS_GetQueryParameter( struct CS_ClientInfo *info, const char *name );
bool CS_SetReplyHeader( struct CS_Reply *reply, const char *header, const char *value );
bool CS_SetReplyHeaderInt( struct CS_Reply *reply, const char *header, int value );
bool CS_SetReplyHeaderIfMissing( struct CS_Reply *reply, const char *header, const char *value );
bool CS_SetReplyHeaderIntIfMissing( struct CS_Reply *reply, const char *header, int value );

struct CS_Reply *CS_Reply( struct CS_ClientInfo *info, int responseEnum, int mimeEnum, void *replyBuffer, int replyLength );
void CS_ReturnReply( struct CS_ClientInfo *info, struct CS_Reply *reply );
bool CS_DoReply( struct CS_ClientInfo *info, struct CS_Reply *reply );

#ifdef __cplusplus
}
#endif
#endif
