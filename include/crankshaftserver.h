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

enum CrankshaftHttpMethods {
    METHOD_CONNECT = 0,
    METHOD_DELETE,
    METHOD_GET,
    METHOD_HEAD,
    METHOD_POST,
    METHOD_PUT,
    METHOD_TRACE,
    METHOD_UNKNOWN,
    CRANKSHAFT_MAX_METHODS = METHOD_UNKNOWN
};

enum CrankshaftMIMETypes {
    MIME_DO_NOT_SET = -1,
    MIME_AAC,
    MIME_APNG,
    MIME_AVI,
    MIME_AZW,
    MIME_BIN,
    MIME_BMP,
    MIME_BZ,
    MIME_BZ2,
    MIME_CSS,
    MIME_GIF,
    MIME_HTM,
    MIME_HTML,
    MIME_ICO,
    MIME_JPG,
    MIME_JPEG,
    MIME_JS,
    MIME_MP3,
    MIME_MP4,
    MIME_OGA,
    MIME_OGV,
    MIME_OGX,
    MIME_OTF,
    MIME_PNG,
    MIME_PDF,
    MIME_RAR,
    MIME_RTF,
    MIME_SVG,
    MIME_TAR,
    MIME_TTF,
    MIME_TXT,
    MIME_WAV,
    MIME_WEBA,
    MIME_WEBM,
    MIME_WEBP,
    MIME_WOFF,
    MIME_WOFF2,
    MIME_XML,
    MIME_FORM_URLENCODED,
    MIME_FORM_MULTIPART,
    MAX_MIME_TYPES
};

enum CrankshaftHTTPResponseCodes {
    RESPONSE_100,
    RESPONSE_101,
    RESPONSE_102,
    RESPONSE_103,
    RESPONSE_200,
    RESPONSE_201,
    RESPONSE_202,
    RESPONSE_203,
    RESPONSE_204,
    RESPONSE_205,
    RESPONSE_206,
    RESPONSE_207,
    RESPONSE_208,
    RESPONSE_226,
    RESPONSE_300,
    RESPONSE_301,
    RESPONSE_302,
    RESPONSE_303,
    RESPONSE_304,
    RESPONSE_305,
    RESPONSE_306,
    RESPONSE_307,
    RESPONSE_308,
    RESPONSE_400,
    RESPONSE_401,
    RESPONSE_402,
    RESPONSE_403,
    RESPONSE_404,
    RESPONSE_405,
    RESPONSE_406,
    RESPONSE_407,
    RESPONSE_408,
    RESPONSE_409,
    RESPONSE_410,
    RESPONSE_411,
    RESPONSE_412,
    RESPONSE_413,
    RESPONSE_414,
    RESPONSE_415,
    RESPONSE_416,
    RESPONSE_417,
    RESPONSE_418,
    RESPONSE_421,
    RESPONSE_422,
    RESPONSE_423,
    RESPONSE_424,
    RESPONSE_425,
    RESPONSE_426,
    RESPONSE_428,
    RESPONSE_429,
    RESPONSE_431,
    RESPONSE_451,
    RESPONSE_500,
    RESPONSE_501,
    RESPONSE_502,
    RESPONSE_503,
    RESPONSE_504,
    RESPONSE_507,
    RESPONSE_508,
    RESPONSE_510,
    RESPONSE_511,
    MAX_NUM_RESPONSE_ENUMS
};


struct CrankshaftWebServer {
    int listenSocket;
    pthread_t serverThread;
    bool killMe;
    bool threadRunning;
    const char *keyPath;
    const char *certificatePath;
    const char *defaultFileServingPath;
    const char *defaultFileServingFile;
    void *replyStack;
    int routeNumbers[CRANKSHAFT_MAX_METHODS];
    struct CrankshaftRoute *routes[CRANKSHAFT_MAX_METHODS];
    SSL_CTX *sslctx;
};

struct RequestHeader {
    const char *header;
    const char *values;
};

#define HEADER_MAX 64
#define HEADER_VALUE_MAX 256
struct ReplyHeader {
    char header[ HEADER_MAX ];
    char value[ HEADER_VALUE_MAX ];
};

struct QueryParameter {
    const char *name;
    const char *value;
};

#define MAX_REQUEST_HEADERS 64
#define MAX_QUERY_PARAMETERS 64

struct CrankshaftRequestInfo {
    bool valid;
    int numHeaders;
    int numParameters;
    const char *uri;
    const char *method;
    const char *httpVersion;
    int requestMethodEnum;
    struct RequestHeader headers[ MAX_REQUEST_HEADERS ];
    struct QueryParameter parameters[ MAX_QUERY_PARAMETERS ];
};

struct CrankshaftClientInfo {
    int clientSocket;
    struct CrankshaftWebServer *server;
    struct sockaddr_in clientSocketAddress;
    struct CS_PushPullBuffer *buffer;
    struct CS_PushPullBuffer *output;
    void (*disconnectCallback)(struct CrankshaftClientInfo *info);
    void *persistentData;
    SSL *ssl;
    struct CrankshaftRequestInfo requestInfo;
};

enum {
    ROUTE_TYPE_WILDCARD,
    ROUTE_TYPE_PREFIX,
    ROUTE_TYPE_EXACT
};

struct CrankshaftRoute {
    int method;
    int routeType;
    int routeLength;
    const char *route;
    bool (*handler)(struct CrankshaftClientInfo *);
};

struct CrankshaftReply {
    int returnStatusEnum;
    int contentTypeEnum;
    int numHeaders;
    struct ReplyHeader replyHeaders[MAX_REQUEST_HEADERS];
    const void *outputBuffer;
    int outputLength;
};

struct CrankshaftWebServer *CS_StartWebServer(int portNum,
                                           const char *certfile,
                                           const char *keyFile,
                                           const char *fileServingPath,
                                           const char *fileServingFile,
                                           struct CrankshaftRoute *routes,
                                           int nRoutes);
bool CS_KillWebServer(struct CrankshaftWebServer *server);

bool CS_Diagnostic200( struct CrankshaftClientInfo *info );
bool CS_FileServer( struct CrankshaftClientInfo *info );

const char *CS_GetRequestHeader( struct CrankshaftClientInfo *info, const char *header );
const char *CS_GetQueryParameter( struct CrankshaftClientInfo *info, const char *name );
bool CS_SetReplyHeader( struct CrankshaftReply *reply, const char *header, const char *value );
bool CS_SetReplyHeaderInt( struct CrankshaftReply *reply, const char *header, int value );
bool CS_SetReplyHeaderIfMissing( struct CrankshaftReply *reply, const char *header, const char *value );
bool CS_SetReplyHeaderIntIfMissing( struct CrankshaftReply *reply, const char *header, int value );

struct CrankshaftReply *CS_Reply( struct CrankshaftClientInfo *info, int responseEnum, int mimeEnum, void *replyBuffer, int replyLength );
void CS_ReturnReply( struct CrankshaftClientInfo *info, struct CrankshaftReply *reply );
bool CS_DoReply( struct CrankshaftClientInfo *info, struct CrankshaftReply *reply );

#ifdef __cplusplus
}
#endif
#endif
