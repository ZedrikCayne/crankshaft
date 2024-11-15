#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <stdbool.h>

#include "crankshaftlogger.h"
#include "crankshaftpushpull.h"
#include "crankshaftalloc.h"
#include "crankshaftserver.h"
#include "crankshaftjson.h"
#include "crankshafttempbuff.h"
#include "crankshaftslaballoc.h"

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

struct ExtensionToMIME {
    char *extension;
    char *filetype;
};

struct ExtensionToMIME extensions[] = {
    {"aac", "audio/aac"},
    {"apng", "image/apng"},
    {"avi", "video/x-msvideo"},
    {"azw", "applicatoin/vnd.amazon.ebook"},
    {"bin", "application/octet-stream"},
    {"bmp", "image/bmp"},
    {"bz", "application/x-bzip"},
    {"bz2", "application/x-bzip2"},
    {"css", "text/css"},
    {"gif", "image/gif"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"ico", "image/vnd.microsoft.icon"},
    {"jpg", "image/jpg"},
    {"jpeg", "image/jpeg"},
    {"js", "text/javascript"},
    {"mp3", "audio/mpeg"},
    {"mp4", "video/mp4"},
    {"oga", "audio/ogg"},
    {"ogv", "video/ogg"},
    {"ogx", "application/ogg"},
    {"otf", "font/otf"},
    {"png", "image/png"},
    {"pdf", "application/pdf"},
    {"rar", "application/vnd.rar"},
    {"rtf", "application/rtf"},
    {"svg", "image/svg+xml"},
    {"tar", "application/x-tar"},
    {"ttf", "font/ttf"},
    {"txt", "text/plain"},
    {"wav", "audio/wav"},
    {"weba", "audio/webm"},
    {"webm", "video/webm"},
    {"webp", "image/webp"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"xml", "applicatoin/xml"},
    {"xxx", "application/x-www-form-urlencoded" },
    {"xxy", "multipart/form-data" }
};

static const char *dayOfWeek[ 7 ] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static const char *month[ 12 ] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static const char *timeString(time_t currentTime) {
    char *returnValue = CS_tempBuff(256);
    struct tm resultTime;
    struct tm *rt = gmtime_r(&currentTime,&resultTime);
    if( rt ) {
        int realYear = rt->tm_year + 1900;
        returnValue[ 0 ] = dayOfWeek[ rt->tm_wday ][0];
        returnValue[ 1 ] = dayOfWeek[ rt->tm_wday ][1];
        returnValue[ 2 ] = dayOfWeek[ rt->tm_wday ][2];
        returnValue[ 3 ] = ',';
        returnValue[ 4 ] = ' ';
        returnValue[ 5 ] = (char)('0' + (rt->tm_mday / 10));
        returnValue[ 6 ] = (char)('0' + (rt->tm_mday % 10));
        returnValue[ 7 ] = ' ';
        returnValue[ 8 ] = month[ rt->tm_mon ][ 0 ];
        returnValue[ 9 ] = month[ rt->tm_mon ][ 1 ];
        returnValue[ 10 ] = month[ rt->tm_mon ][ 2 ];
        returnValue[ 11 ] = ' ';
        returnValue[ 12 ] = (char)('0' + ((realYear/1000) % 10));
        returnValue[ 13 ] = (char)('0' + ((realYear/100) % 10));
        returnValue[ 14 ] = (char)('0' + ((realYear/10) % 10));
        returnValue[ 15 ] = (char)('0' + ((realYear/1) % 10));
        returnValue[ 16 ] = ' ';
        returnValue[ 17 ] = (char)('0' + ((rt->tm_hour) / 10));
        returnValue[ 18 ] = (char)('0' + ((rt->tm_hour) % 10));
        returnValue[ 19 ] = ':';
        returnValue[ 20 ] = (char)('0' + ((rt->tm_min) / 10));
        returnValue[ 21 ] = (char)('0' + ((rt->tm_min) % 10));
        returnValue[ 22 ] = ':';
        returnValue[ 23 ] = (char)('0' + ((rt->tm_sec) / 10));
        returnValue[ 24 ] = (char)('0' + ((rt->tm_sec) % 10));
        returnValue[ 25 ] = ' ';
        returnValue[ 26 ] = 'G';
        returnValue[ 27 ] = 'M';
        returnValue[ 28 ] = 'T';
        returnValue[ 29 ] = 0;
    }
    return returnValue;
}

#define MAX_QUEUE 64
#define CLIENT_RECEIVE_BUFFER 8192 
#define CLIENT_SEND_BUFFER 8192
static void *clientThread(void *var);

static struct CrankshaftClientInfo *createClientInfoWithThread( int socket,
                                                      struct CrankshaftWebServer *server,
                                                      struct sockaddr_in *clientSocketAddress ) {
    struct CrankshaftClientInfo *ci = CS_alloc(sizeof(struct CrankshaftClientInfo));
    if( ci == NULL ) {
        CS_LOG_ERROR( "Out of memory allocating a new client info." );
        goto CLIENT_ERR_OOM;
    }
    ci->server = server;
    ci->buffer = CS_PP_defaultAlloc(CLIENT_RECEIVE_BUFFER);
    if( ci->buffer == NULL ) {
        CS_LOG_ERROR( "Out of memory allocating client buffer." );
        goto CLIENT_ERR_INPUT_BUFF;
    }
    ci->output = CS_PP_defaultAlloc(CLIENT_SEND_BUFFER);
    if( ci->output == NULL ) {
        CS_LOG_ERROR( "Out of memory allocating client buffer." );
        goto CLIENT_ERR_OUTPUT_BUFF;
    }
    ci->clientSocket = socket;
    memcpy( &ci->clientSocketAddress, clientSocketAddress, sizeof(struct sockaddr_in) );
    pthread_t newThread;
    ci->disconnectCallback = NULL;
    ci->persistentData = NULL;
    int result = pthread_create( &newThread, NULL, clientThread, ci );
    if( result < 0 ) {
        CS_LOG_ERROR( "Failed to create client thread." );
        goto CLIENT_ERR_PTHREAD;
    }
    pthread_detach( newThread );
    return ci;
CLIENT_ERR_PTHREAD:
    CS_PP_defaultFree(ci->output);
CLIENT_ERR_OUTPUT_BUFF:
    CS_PP_defaultFree(ci->buffer);
CLIENT_ERR_INPUT_BUFF:
    CS_free(ci);
CLIENT_ERR_OOM:
    return NULL;
}

static bool HTTP_STATE_MACHINE(struct CrankshaftClientInfo *info);

static void *clientThread(void *var) {
    struct CrankshaftClientInfo *clientInfo = (struct CrankshaftClientInfo *)var;
    CS_LOG_TRACE("ClientInfo %p starting.", clientInfo );
    if( clientInfo->server->sslctx ) { 
        clientInfo->ssl = SSL_new( clientInfo->server->sslctx );
        if( clientInfo->ssl == NULL ) {
            CS_LOG_ERROR("Failed to create new ssl.");
            goto CLIENT_BAIL_NOSSL;
        }
        SSL_set_fd( clientInfo->ssl, clientInfo->clientSocket );
        if( SSL_accept( clientInfo->ssl ) <= 0 ) {
            CS_LOG_ERROR("Failed to accept new ssl.");
            ERR_print_errors_fp(stderr);
            goto CLIENT_BAIL_NOSSL;
        }
    } else {
        clientInfo->ssl = NULL;
    }
    while(true) {
        ssize_t bytesRead = clientInfo->ssl?
            CS_PP_readFromSSL(clientInfo->buffer,clientInfo->ssl):
            CS_PP_readFromFile(clientInfo->buffer,clientInfo->clientSocket);
        
        if( bytesRead <= 0 ) {
            break;
        } else {
            if( HTTP_STATE_MACHINE(clientInfo) )
                break;
        }
    }
CLIENT_BAIL_NOSSL:
    if( clientInfo->ssl ) {
        SSL_free( clientInfo->ssl );
        clientInfo->ssl = NULL;
    }
    close(clientInfo->clientSocket);
    if( clientInfo->disconnectCallback != NULL ) {
        clientInfo->disconnectCallback( clientInfo );
        clientInfo->disconnectCallback = NULL;
    }
    if( clientInfo->persistentData!= NULL ) {
        CS_free( clientInfo->persistentData );
        clientInfo->persistentData = NULL;
    }
    CS_PP_defaultFree(clientInfo->buffer);
    CS_PP_defaultFree(clientInfo->output);
    CS_free(clientInfo);
    CS_LOG_TRACE("ClientInfo stopping %p", clientInfo);
    pthread_exit(NULL);
    return NULL;
}

static void freeRoutes(struct CrankshaftWebServer *server) {
    for( int i = 0; i < CRANKSHAFT_MAX_METHODS; ++i ) {
        if( server->routes[ i ] != NULL )
            CS_free( server->routes[ i ] );
        server->routes[ i ] = NULL;
    }
}

static bool InitSSL(struct CrankshaftWebServer *server, const char *certFile,const char *keyFile ) {
    const SSL_METHOD *method;
    method = TLS_server_method();
    server->sslctx = SSL_CTX_new(method);
    if( server->sslctx ) {
        if( SSL_CTX_use_certificate_file( server->sslctx, certFile, SSL_FILETYPE_PEM) <= 0 ) {
            ERR_print_errors_fp(stderr);
            return true;
        }
        if( SSL_CTX_use_PrivateKey_file( server->sslctx, keyFile, SSL_FILETYPE_PEM) <= 0 ) {
            ERR_print_errors_fp(stderr);
            return true;
        }
    }
    return server->sslctx == NULL;
}

static bool DestroySSL(struct CrankshaftWebServer *server) {
    if( server->sslctx ) { 
        SSL_CTX_free(server->sslctx);
        server->sslctx = NULL;
    }
    return false;
}

static void *serverThreadProc(void *var) {
    struct CrankshaftWebServer *server = (struct CrankshaftWebServer *)var;
    server->threadRunning = true;
    server->killMe = false;
    while(server->threadRunning && !server->killMe ) {
        struct pollfd pollMe = {server->listenSocket, POLLIN, 0};
        pollMe.revents = 0;
        int pollVal = poll(&pollMe, 1, 500);
        if( pollVal < 0 ) break;
        if( pollVal == 1 && pollMe.revents == POLLIN ) {
            struct sockaddr_in clientSocketAddress = {0};
            socklen_t addrSize = sizeof(clientSocketAddress);
            int newSock = accept(server->listenSocket, (struct sockaddr *)&clientSocketAddress, &addrSize);
            if( newSock < 0 ) {
                int error = errno;
                CS_LOG_ERROR("Socket closed, error %s", strerror(error));
                server->threadRunning = false;
            } else {
                createClientInfoWithThread( newSock, server, &clientSocketAddress );
            }
        }
    }
    shutdown( server->listenSocket, SHUT_RDWR );
    close( server->listenSocket );
    DestroySSL( server );
    freeRoutes( server );
    server->listenSocket = -1;
    server->threadRunning = false;
    pthread_exit( NULL );
    return NULL;
}

static const char ReplyStackName[] = "Reply Stack";

struct CrankshaftWebServer *CS_StartWebServer(int portNum, 
                                           const char *certFile,
                                           const char *keyFile,
                                           const char *fileServingPath,
                                           const char *fileServingFile,
                                           struct CrankshaftRoute *routes,
                                           int numberOfRoutes ) {
    struct CrankshaftWebServer *returnValue = CS_alloc( sizeof( struct CrankshaftWebServer ) );
    if( returnValue == NULL ) {
        CS_LOG_ERROR("Could not allocate enough for a web server.");
        return NULL;
    }

    returnValue->defaultFileServingPath = fileServingPath;
    returnValue->defaultFileServingFile = fileServingFile;
    returnValue->killMe = false;
    returnValue->threadRunning = false;
    
    int i = 0;
    for( i = 0; i < CRANKSHAFT_MAX_METHODS; ++i ) {
        returnValue->routeNumbers[ i ] = 0;
        returnValue->routes[ i ] = NULL;
    }

    for( i = 0; i < numberOfRoutes; ++i ) {
        int currentIndex = routes[ i ].method;
        if( currentIndex < 0 || currentIndex >= CRANKSHAFT_MAX_METHODS ) {
            CS_LOG_ERROR("Method defined in routes for web server is outside of allowed range.");
            goto ERR_ALLOC;
        }
        returnValue->routeNumbers[ currentIndex ]++;
        int routeLength = strlen(routes[ i ].route);
        routes[ i ].routeLength = routeLength;
    }

    for( i = 0; i < CRANKSHAFT_MAX_METHODS; ++i ) {
        returnValue->routes[ i ] = CS_alloc( sizeof( struct CrankshaftRoute ) * returnValue->routeNumbers[ i ] );
        if( returnValue->routes[ i ] == NULL ) {
            CS_LOG_ERROR("OOM creating routes table.");
            goto ERR_ALLOC_TABLES;
        }
    }

    int routeCount[ CRANKSHAFT_MAX_METHODS ] = {0};
    for( i = 0; i < numberOfRoutes; ++i ) {
        int neededRoute = routes[ i ].method;
        int currentWriteIndex = routeCount[ neededRoute ];

        memcpy( returnValue->routes[ neededRoute ] + currentWriteIndex,
                routes + i,
                sizeof( struct CrankshaftRoute ) );

        ++routeCount[ neededRoute ];
    }

    returnValue->replyStack = CS_initSlabAlloc( ReplyStackName, sizeof( struct CrankshaftReply ), 256, 4 );


    returnValue->listenSocket = socket(AF_INET, SOCK_STREAM,0);
    if( returnValue->listenSocket < 0 ) {
        CS_LOG_ERROR("Failed to open socket for listening.");
        goto ERR_ALLOC_TABLES;
    }

    int optVal = 1;

    if( setsockopt( returnValue->listenSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal) ) < 0 ) {
        CS_LOG_ERROR("Failed to set socket options.");
        goto ERR_SOCK;
    }
    
    struct sockaddr_in serverAddress = {0};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(portNum);
    if( bind( returnValue->listenSocket, (struct sockaddr *)&serverAddress,sizeof(serverAddress) ) < 0 ) {
        CS_LOG_ERROR("Failed to bind socket to port %d.", portNum);
        goto ERR_SOCK;
    }

    if( listen(returnValue->listenSocket,MAX_QUEUE) < 0 ) {
        CS_LOG_ERROR("Failed to listen.");
        goto ERR_SOCK;
    }

    if( certFile != NULL && keyFile != NULL  ) {
        if( InitSSL( returnValue, certFile, keyFile ) ) {
            CS_LOG_ERROR("Failed to init SSL");
            goto ERR_SOCK;
        }
    } else {
        returnValue->sslctx = NULL;
    }

    int result = pthread_create(&returnValue->serverThread, NULL, serverThreadProc, returnValue);
    if( result < 0 ) {
        CS_LOG_ERROR("Thread failed to create.\n");
        goto ERR_SSL;
    }
    pthread_detach(returnValue->serverThread);

    return returnValue;
ERR_SSL:
    DestroySSL( returnValue );
ERR_SOCK:
    close(returnValue->listenSocket);
ERR_ALLOC_TABLES:
    freeRoutes(returnValue);
ERR_ALLOC:
    CS_free(returnValue);
    return NULL;
}

bool CS_KillWebServer( struct CrankshaftWebServer *server ) {
    CS_LOG("Webserver shutdown requested.");
    server->killMe = true;
    while( server->threadRunning ) {
        sleep(1);
    }
    const char *slabAllocDesc = CS_descSlabAlloc(server->replyStack);
    CS_LOG_TRACE("%s", slabAllocDesc);
    CS_free( (void*)slabAllocDesc );
    CS_freeSlabAlloc(server->replyStack);
    CS_free(server);
    return false;
}

#define HTTP_VERSION "HTTP/1.1"

#define STACK_BUFFER_SIZE 1024
static void ERR(struct CrankshaftClientInfo *info, int errorEnum, const char *details) {
    char *tempBuff = CS_tempBuff(STACK_BUFFER_SIZE);
    int error = codeToString[errorEnum].code;
    const char *errorString = codeToString[errorEnum].value;

    int contentLength = snprintf(tempBuff, STACK_BUFFER_SIZE, "{\"error\":\"%s\",\"status\":%d,\"details\":\"%s\"}",errorString,error,errorString);
    struct CrankshaftReply *reply = CS_Reply( info, errorEnum, MIME_JS, tempBuff, contentLength );
    if( reply == NULL ) return;
    CS_DoReply( info, reply );
}

#define TEMP_OUTPUT_BUFF_SIZE 8192 
static bool BASIC_OK(struct CrankshaftClientInfo *info, const char *what) {
    struct CS_PushPullBuffer tempToWriteBase;
    struct CS_PushPullBuffer *tempToWrite = &tempToWriteBase;
    CS_PP_init(tempToWrite,TEMP_OUTPUT_BUFF_SIZE,CS_tempBuff(TEMP_OUTPUT_BUFF_SIZE));
    struct CS_StringBuilder *scratch = CS_SB_create( TEMP_OUTPUT_BUFF_SIZE );
    if( tempToWrite == NULL )
        return true;
    CS_PP_printf( tempToWrite, "{\"what\":\"%s\",\"status\":%d,\"headers\":[",what,200);
    for( int i = 0; i < info->requestInfo.numHeaders; ++i ) { 
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_quoteStringToStringBuilder(info->requestInfo.headers[i].values,1024,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%s\",\"value\":\"%s\"}", 
                info->requestInfo.headers[i].header, scratch->buffer );
    }
    CS_PP_printf( tempToWrite, "],\"queryParams\":[" );
    for( int i = 0; i < info->requestInfo.numParameters; ++i ) {
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_quoteStringToStringBuilder(info->requestInfo.parameters[i].value,1024,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%s\",\"value\":\"%s\"}",
                info->requestInfo.parameters[i].name, scratch->buffer );
    }
    CS_SB_reset( scratch );
    CS_quoteStringToStringBuilder(info->requestInfo.uri,1024,scratch);
    CS_PP_printf( tempToWrite, "],\"dataLeftInBuffer\":%d,\"uri\":\"%s\"}", CS_PP_dataSize( info->buffer ), scratch->buffer );
    CS_SB_free( scratch );
    struct CrankshaftReply *reply = CS_Reply( info, RESPONSE_200, MIME_JS, CS_PP_startOfData( tempToWrite ), CS_PP_dataSize( tempToWrite ) );
    return CS_DoReply( info, reply );
}

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define _CONNECT 0x404f4e4e
#define _DELETE  0x44454c45
#define _GET     0x47455420
#define _HEAD    0x48454144
#define _POST    0x504f5d54
#define _PUT     0x50555420
#define _TRACE   0x54524143
#else
#define _CONNECT 0x43434340
#define _DELETE  0x454c4544
#define _GET     0x20544547
#define _HEAD    0x44414548
#define _POST    0x54534f50
#define _PUT     0x20545550
#define _TRACE   0x43415254
#endif

enum HeaderState {
    HEADER_STATE_POSSIBLE_WHITE_SPACE,
    HEADER_STATE_METHOD,
    HEADER_STATE_URI,
    HEADER_STATE_QUERY_PARAM,
    HEADER_STATE_QUERY_PARAM_NAME,
    HEADER_STATE_QUERY_PARAM_VALUE,
    HEADER_STATE_FRAGMENT,
    HEADER_STATE_HTTP,
    HEADER_STATE_HEADERS,
    HEADER_STATE_HEADER_NAME,
    HEADER_STATE_HEADER_SEPARATOR,
    HEADER_STATE_HEADER_VALUE,
    HEADER_STATE_DONE
};

#define LF ((char)10)
#define CR ((char)13)
#define HT ((char)9)
#define SP ((char)32)
#define AMPERSAND '&'
#define QUESTION '?'
#define POUND '#'
#define COLON ':'
#define EQUAL '='

#define EAT_CRLF() if(*currentPoint==CR){++currentPoint;REQUIRE_CHAR(LF);}
#define REQUIRE_CHAR(X) if( currentPoint<endOfData && *currentPoint==X)++currentPoint;else return -1;
#define REQUIRE_CHAR_NO_EAT(X) if( currentPoint<endOfData && *currentPoint!=X)return -1;
#define REQUIRE_CRLF() REQUIRE_CHAR(CR);REQUIRE_CHAR_NO_EAT(LF)

static int parseRequest(struct CrankshaftClientInfo *info) {
    char *startOfData = CS_PP_startOfData(info->buffer);
    //Find first space, that's the end of the 'method'
    int command = *(int*)startOfData;
    switch(command) {
        case _CONNECT:
            info->requestInfo.requestMethodEnum = METHOD_CONNECT;
            break;
        case _DELETE:
            info->requestInfo.requestMethodEnum = METHOD_DELETE;
            break;
        case _GET:
            info->requestInfo.requestMethodEnum = METHOD_GET;
            break;
        case _HEAD:
            info->requestInfo.requestMethodEnum = METHOD_HEAD;
            break;
        case _POST:
            info->requestInfo.requestMethodEnum = METHOD_POST;
            break;
        case _PUT:
            info->requestInfo.requestMethodEnum = METHOD_PUT;
            break;
        case _TRACE:
            info->requestInfo.requestMethodEnum = METHOD_TRACE;
            break;
        default:
            info->requestInfo.requestMethodEnum = METHOD_UNKNOWN;
            return -1;
    }

    char *currentPoint = startOfData;
    char *startOfToken = NULL;
    char *endOfData = CS_PP_endOfData(info->buffer);
    int currentHeaderState = HEADER_STATE_POSSIBLE_WHITE_SPACE;
    int currentHeaderIndex = 0;
    int currentParameterIndex = 0;
    for( currentPoint = startOfData; currentPoint < endOfData; ++currentPoint) {
        if( startOfToken == NULL ) startOfToken = currentPoint;
        switch( currentHeaderState ) {
            case HEADER_STATE_POSSIBLE_WHITE_SPACE:
                //Stupidly...there might be CR/LF pairs before the actual method...eat them.
                startOfToken = currentPoint;
                EAT_CRLF();
                currentHeaderState = HEADER_STATE_METHOD;
            case HEADER_STATE_METHOD:
                //One space separates method and the uri
                if( *currentPoint == SP )  {
                    *currentPoint = 0;
                    info->requestInfo.method = startOfToken;
                    currentHeaderState = HEADER_STATE_URI;
                    startOfToken = NULL;
                }
                break;
            case HEADER_STATE_URI:
                if( *currentPoint == SP || *currentPoint == QUESTION )  {
                    currentHeaderState = (*currentPoint==SP)?HEADER_STATE_HTTP:HEADER_STATE_QUERY_PARAM;
                    info->requestInfo.uri = startOfToken;
                    if( info->requestInfo.uri[0] != '/' ) {
                        return -1;
                    }
                    startOfToken = NULL;
                    *currentPoint = 0;
                }
                break;
            case HEADER_STATE_QUERY_PARAM:
                if( *currentPoint == AMPERSAND) {
                    startOfToken = NULL;
                    break;
                }
                if( *currentPoint == POUND ) {
                    startOfToken = NULL;
                    currentHeaderState = HEADER_STATE_FRAGMENT;
                    break;
                }
                if( *currentPoint == SP ) {
                    startOfToken = NULL;
                    currentHeaderState = HEADER_STATE_HTTP;
                    break;
                }
                if(currentParameterIndex >= MAX_QUERY_PARAMETERS) {
                    return -1;
                }
                currentHeaderState = HEADER_STATE_QUERY_PARAM_NAME;
            case HEADER_STATE_QUERY_PARAM_NAME:
                if( *currentPoint == AMPERSAND || *currentPoint == EQUAL ) {
                    currentHeaderState = (*currentPoint==AMPERSAND)
                        ?HEADER_STATE_QUERY_PARAM
                        :HEADER_STATE_QUERY_PARAM_VALUE;
                    *currentPoint = 0;
                    info->requestInfo.parameters[ currentParameterIndex ].name = startOfToken;
                    startOfToken = NULL;
                    break;
                }
                break;
            case HEADER_STATE_QUERY_PARAM_VALUE:
                if( *currentPoint == AMPERSAND || *currentPoint == POUND || *currentPoint == SP ) {
                    info->requestInfo.parameters[ currentParameterIndex ].value = startOfToken;
                    ++currentParameterIndex;
                    startOfToken = NULL;
                    currentHeaderState = (*currentPoint==AMPERSAND)
                        ?HEADER_STATE_QUERY_PARAM
                        :(*currentPoint==POUND)
                            ?HEADER_STATE_FRAGMENT
                            :HEADER_STATE_HTTP;
                    *currentPoint = 0;
                    break;
                }
                break;
            case HEADER_STATE_HTTP:
                REQUIRE_CHAR('H');
                REQUIRE_CHAR('T');
                REQUIRE_CHAR('T');
                REQUIRE_CHAR('P');
                REQUIRE_CHAR('/');
                REQUIRE_CHAR('1');
                REQUIRE_CHAR('.');
                REQUIRE_CHAR('1');
                REQUIRE_CRLF();
                startOfToken = NULL;
                currentHeaderState = HEADER_STATE_HEADERS;
                break;
            case HEADER_STATE_HEADERS:
                if( currentPoint + 1 < endOfData && *currentPoint == CR && *(currentPoint + 1) == LF ) {
                    currentPoint += 2;
                    currentHeaderState = HEADER_STATE_DONE;
                    info->requestInfo.numHeaders = currentHeaderIndex;
                    info->requestInfo.numParameters = currentParameterIndex;
                    return currentPoint - startOfData;
                }
                if( currentHeaderIndex >= MAX_REQUEST_HEADERS ) return -1;
                currentHeaderState = HEADER_STATE_HEADER_NAME;
            case HEADER_STATE_HEADER_NAME:
                if( *currentPoint == COLON ) {
                    *currentPoint = 0;
                    info->requestInfo.headers[ currentHeaderIndex ].header = startOfToken;
                    startOfToken = NULL;
                    currentHeaderState = HEADER_STATE_HEADER_SEPARATOR;
                }
                break;
            case HEADER_STATE_HEADER_SEPARATOR:
                if( *currentPoint == SP ) {
                    startOfToken = NULL;
                    break;
                }
                currentHeaderState = HEADER_STATE_HEADER_VALUE;
            case HEADER_STATE_HEADER_VALUE:
                //A CRLF followed by a space or horizontal tab means the value continues
                if( *currentPoint == CR ) {
                    if( currentPoint + 1 < endOfData && *(currentPoint + 1) == LF ) {
                        if( currentPoint + 2 < endOfData ) {
                            if( *(currentPoint + 2) == SP || *(currentPoint + 2) == HT ) {
                                currentPoint += 2;
                                continue;
                            } else {
                                *currentPoint = 0;
                                info->requestInfo.headers[ currentHeaderIndex ].values = startOfToken;
                                ++currentHeaderIndex;
                                startOfToken = NULL;
                                ++currentPoint;
                                currentHeaderState = HEADER_STATE_HEADERS;
                            }
                        } else {
                            return -1; 
                        }
                    } else {
                        return -1;
                    }
                }
                break;
        }
    }

    return -1;
}

static bool HTTP_STATE_MACHINE(struct CrankshaftClientInfo *info) {
    //Message starts:
    size_t bytesAvailable = CS_PP_dataSize(info->buffer);
    int bytesRequiredForHeaders = parseRequest(info);
    if( bytesRequiredForHeaders < 0 ) return true;
    //If there's anything left after the headers, set the internal file pointer ahead.
    if( bytesRequiredForHeaders < bytesAvailable ) CS_PP_write(info->buffer,bytesRequiredForHeaders);
    CS_LOG("Request: %s %s",info->requestInfo.method,info->requestInfo.uri);
    int requestEnum = info->requestInfo.requestMethodEnum;
    int nRoutes = info->server->routeNumbers[ requestEnum ];
    struct CrankshaftRoute *routes = info->server->routes[ requestEnum ];

    for( int i = 0; i < nRoutes; ++i ) {
        switch( routes[ i ].routeType ) {
            case ROUTE_TYPE_WILDCARD:
                return routes[ i ].handler( info );
                break;
            case ROUTE_TYPE_PREFIX:
                if( strncmp(info->requestInfo.uri,routes[ i ].route, routes[ i ].routeLength) == 0 )
                    return routes[ i ].handler( info );
                break;
            case ROUTE_TYPE_EXACT:
                if( strcmp(info->requestInfo.uri, routes[ i ].route ) == 0 )
                    return routes[ i ].handler( info );
                break;
        }
    }
    
    ERR(info, RESPONSE_404,"URI not available on this server");
    return true;
}

bool CS_Diagnostic200( struct CrankshaftClientInfo *info ) {
    return BASIC_OK(info, info->requestInfo.method);
}

static const int extensionToMimeEnum(const char *extension ) {
    for( int i = 0; i < sizeof(extensions)/sizeof(extensions[0]); ++i ) {
        if( strcmp( extension, extensions[ i ].extension ) == 0 ) {
            return i;
        }
    }
    return MIME_BIN;
 }

const char *extensionToMimeType(const char *extension) {
    int mimeEnum = extensionToMimeEnum( extension );
    return extensions[ mimeEnum ].filetype;
}

#define MAX_FILE_PATH 2048
bool CS_FileServer( struct CrankshaftClientInfo *info ) {
    struct CrankshaftRequestInfo *request = &info->requestInfo;
    int lengthOfUri = strlen( request->uri );
    char *tempBuff = CS_tempBuff( lengthOfUri );
    strncpy( tempBuff, request->uri, lengthOfUri + 1 );
    char *path = tempBuff;
    const char *filename = NULL;
    const char *extension = NULL;
    //Check for anyone being sneaky about ..
    for( int i = 1; i < lengthOfUri - 1; ++i ) {
        if( tempBuff[i] == '.' && tempBuff[ i - 1 ] == '.' ) {
            ERR(info,RESPONSE_403,"Relative paths not allowed.");
            goto ERR_SETUP;
        }
    }
    if( lengthOfUri > 1 ) {
        for( int i = lengthOfUri - 1; i >= 0; --i ) {
            if( tempBuff[i] == '/' ) {
                tempBuff[i] = 0;
                if( i < lengthOfUri - 1 ) filename = tempBuff + i + 1;
                for( int j = lengthOfUri - 1; j > i; --j ) {
                    if( tempBuff[ j ] == '.' ) {
                        extension = tempBuff + j + 1;
                    }
                }
                break;
            }
        }
    }

    char *fileToOpen = CS_tempBuff(MAX_FILE_PATH);

    if( filename == NULL ) {
        filename = info->server->defaultFileServingFile;
        int len = strlen(filename);
        for( int i = len - 2; i > 1; --i ) {
            if( filename[ i ] == '.' ) { extension = filename + i + 1; break; }
        } 
    }

    int printed = snprintf( fileToOpen, MAX_FILE_PATH, "%s%s/%s", info->server->defaultFileServingPath, path, filename );
    if( printed == MAX_FILE_PATH ) {
        ERR(info, RESPONSE_403,"Requested file path length too long.");
        goto ERR_SETUP;
    }

    int inputFile = open( fileToOpen, O_RDONLY );
    if( inputFile < 0 ) {
        ERR(info, RESPONSE_404,"File Not Found");
        goto ERR_SETUP;
    }

    struct stat statBuff;
    if( fstat( inputFile, &statBuff ) < 0 ) {
        ERR(info, RESPONSE_500,"Cannot stat file. What the heck?");
        goto ERR_FILE_OPENED;
    }
    
    long fileSize = statBuff.st_size;
    long lastModified = statBuff.st_mtime;

    void *fileBuffer = NULL;

    if( request->requestMethodEnum == METHOD_GET ) {
        fileBuffer = CS_alloc(fileSize);
        if(fileBuffer == NULL) {
            char *tBuff = CS_tempBuff(256);
            snprintf(tBuff, 256, "OOM allocating %ld bytes for a file.", fileSize);
            ERR(info, RESPONSE_500, tBuff );
            goto ERR_FILE_OPENED;
        }

        int numBytesRead = 1;
        int numBytesToRead = fileSize;
        while( numBytesRead > 0 && numBytesToRead > 0 ) {
            numBytesRead = read( inputFile, fileBuffer + (fileSize - numBytesToRead), numBytesToRead );
            numBytesToRead -= numBytesRead;
        }
        if( numBytesToRead > 0 ) {
            ERR(info, RESPONSE_500, "Could not read whole file.");
            goto ERR_BUFF_FAILED;
        }
        close( inputFile );
    }

    //Doing this the long way so we have a default set.
    const char *mimeType = extensionToMimeType(extension);
    struct CrankshaftReply *reply = CS_Reply( info, RESPONSE_200, MIME_DO_NOT_SET, fileBuffer, fileSize );
    CS_SetReplyHeader(reply, "Last-Modified", timeString( lastModified ) );
    CS_SetReplyHeader(reply, "Content-Type", mimeType);
    CS_SetReplyHeader(reply, "Connection", "close" );
    CS_DoReply(info,reply);
    CS_free(fileBuffer);
    return true;
ERR_BUFF_FAILED:
    CS_free(fileBuffer);
ERR_FILE_OPENED:
    close( inputFile );
ERR_SETUP:
    
    return true;
}

const char *CS_GetRequestHeader( struct CrankshaftClientInfo *info, const char *header ) {
    struct CrankshaftRequestInfo *request = &info->requestInfo;
    for( int i = 0; i < request->numHeaders; ++i ) {
        if( strncmp(header,request->headers[i].header,HEADER_MAX) == 0 ) {
            return request->headers[i].values;
        }
    }
    return NULL;
}

const char *GetQueryParameter( struct CrankshaftClientInfo *info, const char *name ) {
    struct CrankshaftRequestInfo *request = &info->requestInfo;
    for( int i = 0; i < request->numHeaders; ++i ) {
        if( strncmp(name,request->parameters[i].name,HEADER_MAX) == 0 ) {
            return request->parameters[i].value;
        }
    }
    return NULL;
}

static bool PrivateSetReplyHeader( struct CrankshaftReply *reply,
                                   bool overwrite,
                                   const char *header, 
                                   const char *value ) {
    if( header == NULL || value == NULL ) {
        CS_LOG_ERROR("Trying to set a reply header with a null header or value.");
        return true;
    }
    int headerLen = strlen(header);
    if( headerLen > HEADER_MAX - 1 ) {
        CS_LOG_ERROR("Trying to set a reply header longer than %d", HEADER_MAX-1);
        return true;
    }
    int valueLen = strlen(value);
    if( valueLen > HEADER_VALUE_MAX - 1 ) {
        CS_LOG_ERROR("Trying to set a value in a reply header longer than %d", HEADER_VALUE_MAX-1); 
        return true;
    }

    int indexAlreadySet = -1;
    int indexToSet = reply->numHeaders;

    for( int i = 0; i < reply->numHeaders; ++i ) {
        if( strncmp(header, reply->replyHeaders[i].header, HEADER_MAX) == 0) {
            indexAlreadySet = i;
            indexToSet = i;
        }
    }
    if( indexToSet >= MAX_REQUEST_HEADERS ) {
        CS_LOG_ERROR("Trying to set more than %d headers in a reply.", MAX_REQUEST_HEADERS);
        return true;
    }
    if( indexAlreadySet < 0 || overwrite ) {
        if( indexAlreadySet < 0 ) {
            ++reply->numHeaders;
            strncpy(reply->replyHeaders[indexToSet].header, header, HEADER_MAX);
        }
        strncpy(reply->replyHeaders[indexToSet].value, value, HEADER_VALUE_MAX);
        return false;
    }
    return true;
}

static bool PrivateSetReplyHeaderInt( struct CrankshaftReply *reply, bool overwrite, const char *header, int value ) {
    char *temp = (char*)CS_tempBuff( 64 );
    snprintf( temp, 64, "%d", value );
    return PrivateSetReplyHeader( reply, overwrite, header, temp );
}

struct CrankshaftReply *CS_Reply(struct CrankshaftClientInfo *info, int responseEnum, int mimeEnum, void *outputBuffer, int outputLength ) {
        struct CrankshaftReply *returnValue = CS_takeOne(info->server->replyStack);
    returnValue->returnStatusEnum = responseEnum;
    returnValue->contentTypeEnum = mimeEnum;
    returnValue->numHeaders = 0;
    returnValue->outputBuffer = outputBuffer;
    returnValue->outputLength = outputLength;
    return returnValue;
}

void CS_ReturnReply(struct CrankshaftClientInfo *info, struct CrankshaftReply *reply) {
    CS_returnOne(info->server->replyStack, reply);
}

bool CS_SetReplyHeader( struct CrankshaftReply *reply, const char *header, const char *value ) {
    return PrivateSetReplyHeader(reply,true,header,value);
}
bool CS_SetReplyHeaderIfMissing( struct CrankshaftReply *reply, const char *header, const char *value ) {
    return PrivateSetReplyHeader(reply,false,header,value);
}
bool CS_SetReplyHeaderInt( struct CrankshaftReply *reply, const char *header, int value ) {
    return PrivateSetReplyHeaderInt( reply, true, header, value );
}
bool CS_SetReplyHeaderIntIfMissing( struct CrankshaftReply *reply, const char *header, int value ) {
    return PrivateSetReplyHeaderInt( reply, false, header, value );
}


bool CS_DoReply( struct CrankshaftClientInfo *info, struct CrankshaftReply *reply ) {
    if( info == NULL || reply == NULL || reply->returnStatusEnum < 0 || reply->returnStatusEnum >= MAX_NUM_RESPONSE_ENUMS ) {
        CS_LOG_ERROR("Bad arguments.");
        return true;
    }
    int replyNumber = codeToString[ reply->returnStatusEnum ].code;
    const char *replyString = codeToString[ reply->returnStatusEnum ].value;
    if( reply->contentTypeEnum != MIME_DO_NOT_SET ) {
        CS_SetReplyHeaderIfMissing(reply, "Content-Type", extensions[ reply->contentTypeEnum ].filetype );
    }
    if( reply->outputBuffer != NULL ) {
        CS_SetReplyHeaderIntIfMissing(reply, "Content-Length", reply->outputLength );
    }
    CS_SetReplyHeaderIfMissing(reply, "Date", timeString(time(NULL)));
    CS_SetReplyHeaderIfMissing(reply, "Cache-Control", "no-cache" );
    CS_PP_printf( info->output, "%s %d %s\r\n", HTTP_VERSION, replyNumber, replyString );
    for( int i = 0; i < reply->numHeaders; ++i ) {
        CS_PP_printf( info->output, "%s: %s\r\n", reply->replyHeaders[i].header, reply->replyHeaders[i].value );
    }
    CS_PP_printf( info->output, "\r\n" );

    if( reply->outputBuffer != NULL ) {
        int bytesToWrite = reply->outputLength;
        int bytesWritten = 0;
        int bytesPutInBuff = 0;
        int totalBytesTaken = 0;

        do {
            bytesPutInBuff = CS_PP_readFromBuffer( info->output, ((char*)reply->outputBuffer) + totalBytesTaken, reply->outputLength - totalBytesTaken );
            totalBytesTaken += bytesPutInBuff;
            bytesWritten = info->ssl?
                CS_PP_writeToSSL( info->output, info->ssl ):
                CS_PP_writeToFile( info->output, info->clientSocket );
            bytesToWrite -= bytesWritten;
        } while( bytesToWrite > 0 && bytesWritten > 0 );
    }
    CS_ReturnReply(info, reply);
    return false;
}

