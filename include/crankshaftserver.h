#ifndef __serverdoth__
#define __serverdoth__
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#ifdef __cplusplus
extern "C" {
#endif

enum CS_HttpMethods {
    CS_HTTP_METHOD_CONNECT = 0,
    CS_HTTP_METHOD_DELETE,
    CS_HTTP_METHOD_GET,
    CS_HTTP_METHOD_HEAD,
    CS_HTTP_METHOD_POST,
    CS_HTTP_METHOD_PUT,
    CS_HTTP_METHOD_TRACE,
    CS_HTTP_METHOD_UNKNOWN,
    CS_MAX_HTTP_METHODS = CS_HTTP_METHOD_UNKNOWN
};

enum CS_MIMETypes {
    CS_MIME_DO_NOT_SET = -1,
    CS_MIME_AAC,
    CS_MIME_APNG,
    CS_MIME_AVI,
    CS_MIME_AZW,
    CS_MIME_BIN,
    CS_MIME_BMP,
    CS_MIME_BZ,
    CS_MIME_BZ2,
    CS_MIME_CSS,
    CS_MIME_GIF,
    CS_MIME_HTM,
    CS_MIME_HTML,
    CS_MIME_ICO,
    CS_MIME_JPG,
    CS_MIME_JPEG,
    CS_MIME_JS,
    CS_MIME_MP3,
    CS_MIME_MP4,
    CS_MIME_OGA,
    CS_MIME_OGV,
    CS_MIME_OGX,
    CS_MIME_OTF,
    CS_MIME_PNG,
    CS_MIME_PDF,
    CS_MIME_RAR,
    CS_MIME_RTF,
    CS_MIME_SVG,
    CS_MIME_TAR,
    CS_MIME_TTF,
    CS_MIME_TXT,
    CS_MIME_WAV,
    CS_MIME_WEBA,
    CS_MIME_WEBM,
    CS_MIME_WEBP,
    CS_MIME_WOFF,
    CS_MIME_WOFF2,
    CS_MIME_XML,
    CS_MIME_FORM_URLENCODED,
    CS_MIME_FORM_MULTIPART,
    MAX_CS_MIME_TYPES
};

enum CS_HTTPResponseCodes {
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


struct CS_WebServer {
    int listenSocket;
    pthread_t serverThread;
    bool killMe;
    bool threadRunning;
    const char *keyPath;
    const char *certificatePath;
    const char *defaultFileServingPath;
    const char *defaultFileServingFile;
    void *replyStack;
    int routeNumbers[CS_MAX_HTTP_METHODS];
    struct CS_Route *routes[CS_MAX_HTTP_METHODS];
    SSL_CTX *sslctx;
};

struct CS_RequestHeader {
    const char *header;
    const char *values;
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

struct CS_FormParameters {
    int count;
    const char *buffer;
    const char *queryParameters;
};

#define MAX_REQUEST_HEADERS 64
#define MAX_QUERY_PARAMETERS 64
#define MAX_FORM_PARAMETERS 64

struct CS_RequestInfo {
    bool valid;
    int numHeaders;
    int numParameters;
    const char *uri;
    const char *method;
    const char *httpVersion;
    int requestMethodEnum;
    struct CS_RequestHeader headers[ MAX_REQUEST_HEADERS ];
    struct CS_QueryParameter parameters[ MAX_QUERY_PARAMETERS ];
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
    struct CS_ReplyHeader replyHeaders[MAX_REQUEST_HEADERS];
    const void *outputBuffer;
    int outputLength;
};

struct CS_WebServer *CS_StartWebServer(int portNum,
                                           const char *certfile,
                                           const char *keyFile,
                                           const char *fileServingPath,
                                           const char *fileServingFile,
                                           struct CS_Route *routes,
                                           int nRoutes);
bool CS_KillWebServer(struct CS_WebServer *server);

bool CS_Diagnostic200( struct CS_ClientInfo *info );
bool CS_FileServer( struct CS_ClientInfo *info );

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
