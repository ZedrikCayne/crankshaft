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
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <stdbool.h>

#include <crankshaft/logger.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/alloc.h>
#include <crankshaft/server.h>
#include <crankshaft/json.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/slaballoc.h>
#include <crankshaft/http.h>
#include <crankshaft/mime.h>

static const char *dayOfWeek[ 7 ] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static const char *month[ 12 ] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static const char *timeString(time_t currentTime) {
    char *returnValue = CS_tempBuff(64);
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

static struct CS_ClientInfo *createClientInfoWithThread( int socket,
                                                      struct CS_WebServer *server,
                                                      struct sockaddr_in *clientSocketAddress ) {
    struct CS_ClientInfo *ci = CS_alloc(sizeof(struct CS_ClientInfo));
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

static bool HTTP_STATE_MACHINE(struct CS_ClientInfo *info);

static void *clientThread(void *var) {
    struct CS_ClientInfo *clientInfo = (struct CS_ClientInfo *)var;
    CS_LOG_TRACE("ClientInfo %p starting.", clientInfo );
    if( clientInfo->server->sslctx ) { 
        clientInfo->ssl = SSL_new( clientInfo->server->sslctx );
        if( clientInfo->ssl == NULL ) {
            CS_LOG_ERROR("Failed to create new ssl.");
            goto CLIENT_BAIL_NOSSL;
        }
        SSL_set_fd( clientInfo->ssl, clientInfo->clientSocket );
        if( SSL_accept( clientInfo->ssl ) <= 0 ) {
            //CS_LOG_ERROR("Failed to accept new ssl.");
            //ERR_print_errors_fp(stderr);
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

static void freeRoutes(struct CS_WebServer *server) {
    for( int i = 0; i < CS_MAX_HTTP_METHODS; ++i ) {
        if( server->routes[ i ] != NULL )
            CS_free( server->routes[ i ] );
        server->routes[ i ] = NULL;
    }
}

static EVP_PKEY *ss_pkey = NULL;
static X509 *ss_X509 = NULL;

static bool privateMakeSelfSign(const char *hostname) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if( ctx == NULL ) return true;
    if( EVP_PKEY_keygen_init(ctx) <= 0 ) return true;
    if( EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ) return true;
    if( EVP_PKEY_keygen( ctx, &ss_pkey ) <= 0 ) return true;
    EVP_PKEY_CTX_free(ctx);
    ss_X509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(ss_X509),1);
    X509_gmtime_adj(X509_get_notBefore(ss_X509), 0);
    X509_gmtime_adj(X509_get_notAfter(ss_X509), 31536000L);
    X509_set_pubkey(ss_X509, ss_pkey);
    X509_NAME * name;
    name = X509_get_subject_name(ss_X509);
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
                                       (unsigned char *)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
                                       (unsigned char *)"Just Add Hippo Inc.", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                       (unsigned char *)hostname, -1, -1, 0);
    X509_set_issuer_name(ss_X509, name);
    X509_sign( ss_X509, ss_pkey, EVP_sha256() );
    return false;
}

static bool InitSSL(struct CS_WebServer *server, const char *certFile, const char *keyFile, const char *selfSignHostname ) {
    const SSL_METHOD *method;
    method = TLS_server_method();
    server->sslctx = SSL_CTX_new(method);
    if( server->sslctx ) {
        if( selfSignHostname ) {
            if(privateMakeSelfSign( selfSignHostname )) return true;
            if( SSL_CTX_use_certificate( server->sslctx, ss_X509 ) <= 0 ) {
                ERR_print_errors_fp(stderr);
                return true;
            }
            if( SSL_CTX_use_PrivateKey( server->sslctx, ss_pkey ) <= 0 ) {
                ERR_print_errors_fp(stderr);
                return true;
            }
        } else {
            if( SSL_CTX_use_certificate_file( server->sslctx, certFile, SSL_FILETYPE_PEM) <= 0 ) {
                ERR_print_errors_fp(stderr);
                return true;
            }
            if( SSL_CTX_use_PrivateKey_file( server->sslctx, keyFile, SSL_FILETYPE_PEM) <= 0 ) {
                ERR_print_errors_fp(stderr);
                return true;
            }
        }
    }
    return server->sslctx == NULL;
}

static bool DestroySSL(struct CS_WebServer *server) {
    if( server->sslctx ) { 
        if( ss_X509 ) X509_free( ss_X509 );
        ss_X509 = NULL;
        if( ss_pkey ) EVP_PKEY_free( ss_pkey );
        ss_pkey = NULL;
        SSL_CTX_free(server->sslctx);
        server->sslctx = NULL;
    }
    return false;
}

static void *serverThreadProc(void *var) {
    struct CS_WebServer *server = (struct CS_WebServer *)var;
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
                CS_LOG_ERROR("Socket closed, error %s", strerror(errno));
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

struct CS_WebServer *CS_serverStart(int portNum, 
                                           const char *certFile,
                                           const char *keyFile,
                                           const char *selfSignHostname,
                                           const char *fileServingPath,
                                           const char *fileServingFile,
                                           int fileServingCacheControlMaxAge,
                                           struct CS_Route *routes,
                                           int numberOfRoutes ) {
    struct CS_WebServer *returnValue = CS_alloc( sizeof( struct CS_WebServer ) );
    if( returnValue == NULL ) {
        CS_LOG_ERROR("Could not allocate enough for a web server.");
        return NULL;
    }

    returnValue->defaultFileServingPath = fileServingPath;
    returnValue->defaultFileServingFile = fileServingFile;
    returnValue->killMe = false;
    returnValue->threadRunning = false;
    returnValue->defaultFileServingCacheControlMaxAge = fileServingCacheControlMaxAge;
    
    int i = 0;
    int j = 0;
    for( i = 0; i < CS_MAX_HTTP_METHODS; ++i ) {
        returnValue->routeNumbers[ i ] = 0;
        returnValue->routes[ i ] = NULL;
    }

    for( i = 0; i < numberOfRoutes; ++i ) {
        int currentIndex = routes[ i ].method;
        if( currentIndex < CS_HTTP_METHOD_ANY || currentIndex >= CS_MAX_HTTP_METHODS ) {
            CS_LOG_ERROR("Method defined in routes for web server is outside of allowed range.");
            goto ERR_ALLOC;
        }
        int routeLength = strlen(routes[ i ].route);
        routes[ i ].routeLength = routeLength;
        if( currentIndex == CS_HTTP_METHOD_ANY ) {
            for( j = 0; j < CS_MAX_HTTP_METHODS; ++j ) returnValue->routeNumbers[ j ]++;
        } else {
            returnValue->routeNumbers[ currentIndex ]++;
        }
    }

    for( i = 0; i < CS_MAX_HTTP_METHODS; ++i ) {
        returnValue->routes[ i ] = CS_alloc( sizeof( struct CS_Route ) * returnValue->routeNumbers[ i ] );
        if( returnValue->routes[ i ] == NULL ) {
            CS_LOG_ERROR("OOM creating routes table.");
            goto ERR_ALLOC_TABLES;
        }
    }

    int routeCount[ CS_MAX_HTTP_METHODS ] = {0};
    for( i = 0; i < numberOfRoutes; ++i ) {
        int neededRoute = routes[ i ].method;
        if( neededRoute == CS_HTTP_METHOD_ANY ) {
            for( j = 0; j < CS_MAX_HTTP_METHODS; ++j ) {
                int currentWriteIndex = routeCount[ j ];
                memcpy( returnValue->routes[ j ] + currentWriteIndex, routes + i, sizeof( struct CS_Route) );
                ++routeCount[ j ];
            }
        } else {
            int currentWriteIndex = routeCount[ neededRoute ];

            memcpy( returnValue->routes[ neededRoute ] + currentWriteIndex,
                    routes + i,
                    sizeof( struct CS_Route ) );

            ++routeCount[ neededRoute ];
        }
    }

    returnValue->replyStack = CS_slabInit( ReplyStackName, sizeof( struct CS_Reply ), 256, 8 );

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

    struct sockaddr_in serverAddressPostBind = {0};
    socklen_t addrLen = sizeof(serverAddressPostBind);
    if( getsockname( returnValue->listenSocket, (struct sockaddr *)&serverAddressPostBind, &addrLen ) < 0 ) {
        CS_LOG_ERROR("Failed to get socket address.");
        goto ERR_SOCK;
    }
    returnValue->serverPort = ntohs(serverAddressPostBind.sin_port);

    if( listen(returnValue->listenSocket,MAX_QUEUE) < 0 ) {
        CS_LOG_ERROR("Failed to listen.");
        goto ERR_SOCK;
    }

    if( (certFile != NULL && keyFile != NULL) || selfSignHostname != NULL  ) {

        if( InitSSL( returnValue, certFile, keyFile, selfSignHostname ) ) {
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

bool CS_serverKill( struct CS_WebServer *server ) {
    server->killMe = true;
    while( server->threadRunning ) {
        sleep(1);
    }
    CS_slabFree(server->replyStack);
    CS_free(server);
    return false;
}

#define HTTP_VERSION "HTTP/1.1"

#define STACK_BUFFER_SIZE 1024
static void ERR(struct CS_ClientInfo *info, int errorEnum, const char *details) {
    char *tempBuff = CS_tempBuff(STACK_BUFFER_SIZE);
    int error = CS_httpResponseEnumToCode( errorEnum );
    const char *errorString = CS_httpResponseEnumToString( errorEnum );

    int contentLength = snprintf(tempBuff, STACK_BUFFER_SIZE, "{\"error\":\"%s\",\"status\":%d,\"details\":\"%s\"}",errorString,error,errorString);
    struct CS_Reply *reply = CS_serverCreateReply( info, errorEnum, CS_MIME_JSON, tempBuff, contentLength );
    if( reply == NULL ) return;
    CS_serverDoReply( info, reply );
}

#define TEMP_OUTPUT_BUFF_SIZE 8192 
static bool BASIC_OK(struct CS_ClientInfo *info, const char *what) {
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
        CS_jsonQuoteStringToStringBuilder(info->requestInfo.headers[i].values,1024,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%s\",\"value\":\"%s\"}", 
                info->requestInfo.headers[i].header, scratch->buffer );
    }
    CS_PP_printf( tempToWrite, "],\"queryParameters\":[" );
    for( int i = 0; i < info->requestInfo.numParameters; ++i ) {
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_jsonQuoteStringToStringBuilder(info->requestInfo.parameters[i].value,1024,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%s\",\"value\":\"%s\"}",
                info->requestInfo.parameters[i].name, scratch->buffer );
    }
    CS_SB_reset( scratch );
    CS_jsonQuoteStringToStringBuilder(info->requestInfo.uri,1024,scratch);
    CS_PP_printf( tempToWrite, "],\"dataLeftInBuffer\":%d,\"uri\":\"%s\",\"formParameters\":[", CS_PP_dataSize( info->buffer ), scratch->buffer );
    for( int i = 0; i < info->requestInfo.numFormParameters; ++i ) {
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_jsonQuoteStringToStringBuilder(info->requestInfo.formParameters[i].value,1024,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%s\",\"value\":\"%s\"}",
                info->requestInfo.formParameters[i].name, scratch->buffer );
    }
    CS_PP_printf( tempToWrite, "]}" );
    CS_SB_free( scratch );
    struct CS_Reply *reply = CS_serverCreateReply( info, CS_RESPONSE_200, CS_MIME_JSON, CS_PP_startOfData( tempToWrite ), CS_PP_dataSize( tempToWrite ) );
    return CS_serverDoReply( info, reply );
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
    HEADER_STATE_FORM_NAME,
    HEADER_STATE_FORM_VALUE,
    HEADER_STATE_DONE,
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

#define CS_ClearRequestInfo(X) memset(&((X)->requestInfo),0,sizeof(struct CS_RequestInfo))

static int parseRequest(struct CS_ClientInfo *info) {
    CS_ClearRequestInfo(info);
    char *startOfData = CS_PP_startOfData(info->buffer);
    //Find first space, that's the end of the 'method'
    int command = *(int*)startOfData;
    switch(command) {
        case _CONNECT:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_CONNECT;
            break;
        case _DELETE:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_DELETE;
            break;
        case _GET:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_GET;
            break;
        case _HEAD:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_HEAD;
            break;
        case _POST:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_POST;
            break;
        case _PUT:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_PUT;
            break;
        case _TRACE:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_TRACE;
            break;
        default:
            info->requestInfo.requestMethodEnum = CS_HTTP_METHOD_UNKNOWN;
            return -1;
    }

    char *currentPoint = startOfData;
    char *startOfToken = NULL;
    char *endOfData = CS_PP_endOfData(info->buffer);
    int currentHeaderState = HEADER_STATE_POSSIBLE_WHITE_SPACE;
    int currentHeaderIndex = 0;
    int currentParameterIndex = 0;
    int currentFormParameterIndex = 0;
    const char *contentType = NULL;
    const char *contentLength = NULL;
    const char *endOfContent = NULL;
    int length = 0;

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
                    *currentPoint = 0;
                    if( CS_httpUrlDecodeInPlace( startOfToken ) ) return -1;
                    startOfToken = NULL;
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
                    info->requestInfo.numParameters = currentParameterIndex;
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
                    startOfToken = currentPoint;
                    info->requestInfo.numHeaders = currentHeaderIndex;
                    if( info->requestInfo.requestMethodEnum != CS_HTTP_METHOD_POST ) {
                        return currentPoint - startOfData;
                    }
                    contentType = CS_serverGetRequestHeader( info, "Content-Type" );
                    if( contentType == NULL || strcmp( contentType, "application/x-www-form-urlencoded" ) != 0 ) {
                        return currentPoint - startOfData;
                    }
                    contentLength = CS_serverGetRequestHeader( info, "Content-Length" );
                    if( contentLength == NULL ) {
                        return currentPoint - startOfData;
                    }
                    char *endOfValue;
                    length = strtoll( contentLength, &endOfValue, 10 );
                    if( endOfValue == contentLength ) {
                        return currentPoint - startOfData;
                    }
                    endOfContent = startOfToken + length;
                    //We might not have read anything beyond the header...keep pulling stuff in
                    //until we can't anymore.
                    while( endOfContent > endOfData ) {
                        if( CS_PP_bufferRemaining( info->buffer ) <= 0 ) {
                            CS_LOG_ERROR("Header + Payload for urlencoded form too big.");
                            return -1;
                        }
                        ssize_t bytesRead = info->ssl?
                            CS_PP_readFromSSL(info->buffer,info->ssl):
                            CS_PP_readFromFile(info->buffer,info->clientSocket);
                        if( bytesRead < 0 ) return -1;
                        endOfData = CS_PP_endOfData(info->buffer);
                    }
                    currentHeaderState = HEADER_STATE_FORM_NAME;
                    break;
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
                                info->requestInfo.numHeaders = currentHeaderIndex;
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
           case HEADER_STATE_FORM_NAME:
                if( *currentPoint == EQUAL || currentPoint >= endOfContent ) {
                    *currentPoint = 0;
                    info->requestInfo.formParameters[ currentFormParameterIndex ].name = startOfToken;
                    currentHeaderState = HEADER_STATE_FORM_VALUE;
                    startOfToken = NULL;
                }
                break;
            case HEADER_STATE_FORM_VALUE:
                if( *currentPoint == AMPERSAND || currentPoint + 1 >= endOfContent ) {
                    if( currentPoint + 1 >= endOfContent ) {
                        currentPoint[ 1 ] = 0;
                    } else {
                        *currentPoint = 0;
                    }
                    info->requestInfo.formParameters[ currentFormParameterIndex ].value = startOfToken;
                    if( CS_httpUrlDecodeInPlace( startOfToken ) ) return -1;
                    ++currentFormParameterIndex;
                    info->requestInfo.numFormParameters = currentFormParameterIndex;
                    startOfToken = NULL;
                    if( currentPoint + 1 >= endOfContent ) {
                        return currentPoint + 1 - startOfData;
                    } else {
                        currentHeaderState = HEADER_STATE_FORM_NAME;
                    }
                }
                break;
            case HEADER_STATE_DONE:
                break;
        }
    }

    return -1;
}

static bool HTTP_STATE_MACHINE(struct CS_ClientInfo *info) {
    //Message starts:
    int bytesRequiredForHeaders = parseRequest(info);
    if( bytesRequiredForHeaders < 0 ) return true;
    size_t bytesAvailable = CS_PP_dataSize(info->buffer);
    //If there's anything left after the headers, set the internal file pointer ahead.
    if( bytesRequiredForHeaders < bytesAvailable ) CS_PP_write(info->buffer,bytesRequiredForHeaders);
    CS_LOG_INFO("Request: %s %s",info->requestInfo.method,info->requestInfo.uri);
    int requestEnum = info->requestInfo.requestMethodEnum;
    int nRoutes = info->server->routeNumbers[ requestEnum ];
    struct CS_Route *routes = info->server->routes[ requestEnum ];

    for( int i = 0; i < nRoutes; ++i ) {
        switch( routes[ i ].routeType ) {
            case CS_ROUTE_TYPE_FILTER:
                if( routes[i].handler(info) )
                    return true;
                break;
            case CS_ROUTE_TYPE_WILDCARD:
                return routes[ i ].handler( info );
                break;
            case CS_ROUTE_TYPE_PREFIX:
                if( strncmp(info->requestInfo.uri,routes[ i ].route, routes[ i ].routeLength) == 0 )
                    return routes[ i ].handler( info );
                break;
            case CS_ROUTE_TYPE_EXACT:
                if( strcmp(info->requestInfo.uri, routes[ i ].route ) == 0 )
                    return routes[ i ].handler( info );
                break;
        }
    }
    
    ERR(info, CS_RESPONSE_404, "URI not available on this server");
    return true;
}

bool CS_serverDiagnostic200( struct CS_ClientInfo *info ) {
    return BASIC_OK(info, info->requestInfo.method);
}

#define MAX_FILE_PATH 2048
bool CS_serverFileServer( struct CS_ClientInfo *info ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    int lengthOfUri = strlen( request->uri );
    char *tempBuff = CS_tempBuff( lengthOfUri );
    strncpy( tempBuff, request->uri, lengthOfUri + 1 );
    char *path = tempBuff;
    const char *filename = NULL;
    const char *extension = NULL;
    //Check for anyone being sneaky about ..
    for( int i = 1; i < lengthOfUri - 1; ++i ) {
        if( tempBuff[i] == '.' && tempBuff[ i - 1 ] == '.' ) {
            ERR(info,CS_RESPONSE_403,"Relative paths not allowed.");
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
        ERR(info, CS_RESPONSE_403,"Requested file path length too long.");
        goto ERR_SETUP;
    }

    int inputFile = open( fileToOpen, O_RDONLY );
    if( inputFile < 0 ) {
        ERR(info, CS_RESPONSE_404,"File Not Found");
        goto ERR_SETUP;
    }

    struct stat statBuff;
    if( fstat( inputFile, &statBuff ) < 0 ) {
        ERR(info, CS_RESPONSE_500,"Cannot stat file. What the heck?");
        goto ERR_FILE_OPENED;
    }
    
    long fileSize = statBuff.st_size;
    long lastModified = statBuff.st_mtime;

    void *fileBuffer = NULL;

    if( request->requestMethodEnum == CS_HTTP_METHOD_GET ) {
        fileBuffer = CS_alloc(fileSize);
        if(fileBuffer == NULL) {
            char *tBuff = CS_tempBuff(256);
            snprintf(tBuff, 256, "OOM allocating %ld bytes for a file.", fileSize);
            ERR(info, CS_RESPONSE_500, tBuff );
            goto ERR_FILE_OPENED;
        }

        int numBytesRead = 1;
        int numBytesToRead = fileSize;
        while( numBytesRead > 0 && numBytesToRead > 0 ) {
            numBytesRead = read( inputFile, fileBuffer + (fileSize - numBytesToRead), numBytesToRead );
            numBytesToRead -= numBytesRead;
        }
        if( numBytesToRead > 0 ) {
            ERR(info, CS_RESPONSE_500, "Could not read whole file.");
            goto ERR_BUFF_FAILED;
        }
        close( inputFile );
    }

    //Doing this the long way so we have a default set.
    const char *mimeType = CS_mimeFileExtensionToString(extension);
    struct CS_Reply *reply = CS_serverCreateReply( info, CS_RESPONSE_200, CS_MIME_DO_NOT_SET, fileBuffer, fileSize );
    CS_serverSetReplyHeader(reply, "Last-Modified", timeString( lastModified ) );
    CS_serverSetReplyHeader(reply, "Content-Type", mimeType);
    CS_serverSetReplyHeader(reply, "Connection", "close" );
    if( info->server->defaultFileServingCacheControlMaxAge != 0 ) {
        CS_serverSetReplyHeader(reply, "Cache-Control", CS_tempBuffSnprintf(256,"max-age=%d", info->server->defaultFileServingCacheControlMaxAge) );
    } else {
        CS_serverSetReplyHeader(reply, "Cache-Control", "no-cache" );
    }
    CS_serverDoReply(info,reply);
    CS_free(fileBuffer);
    return true;
ERR_BUFF_FAILED:
    CS_free(fileBuffer);
ERR_FILE_OPENED:
    close( inputFile );
ERR_SETUP:
    
    return true;
}

const char *CS_serverGetRequestHeader( struct CS_ClientInfo *info, const char *header ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int i = 0; i < request->numHeaders; ++i ) {
        if( strncmp(header,request->headers[i].header,HEADER_MAX) == 0 ) {
            return request->headers[i].values;
        }
    }
    return NULL;
}

const char *CS_serverGetRequestQueryParameter( struct CS_ClientInfo *info, const char *name ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int i = 0; i < request->numHeaders; ++i ) {
        if( strncmp(name,request->parameters[i].name,HEADER_MAX) == 0 ) {
            return request->parameters[i].value;
        }
    }
    return NULL;
}

const char *CS_serverGetRequestCookie( struct CS_ClientInfo *info, const char *cookie ) {
    const char *cookieValue = CS_serverGetRequestHeader( info, "Cookie" );
    if( cookie == NULL ) return NULL;
    char *cookieCopy = CS_tempStringCopy( cookieValue );
    char *savePtrOuter;
    char *savePtrInner;
    char *current;
    char *innerCurrent;
    while( (current = strtok_r( cookieCopy, ";", &savePtrOuter )) ) {
        cookieCopy = NULL;
        innerCurrent = strtok_r( current, "=", &savePtrInner );
        if( strcmp( cookie, innerCurrent ) == 0 ) {
            innerCurrent = strtok_r( NULL, "=", &savePtrInner );
            return innerCurrent;
        }
    }
    return NULL;
}

const char *CS_serverGetRequestFormParameter( struct CS_ClientInfo *info, const char *name ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int i = 0; i < request->numFormParameters; ++i ) {
        if( strncmp(name,request->formParameters[i].name,HEADER_MAX) == 0 ) {
            return request->parameters[i].value;
        }
    }
    return NULL;
}

static bool PrivateSetReplyHeader( struct CS_Reply *reply,
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
            indexToSet = reply->numHeaders;
            ++reply->numHeaders;
            strncpy(reply->replyHeaders[indexToSet].header, header, HEADER_MAX);
        }
        strncpy(reply->replyHeaders[indexToSet].value, value, HEADER_VALUE_MAX);
        return false;
    }
    return true;
}

static bool PrivateSetReplyHeaderInt( struct CS_Reply *reply, bool overwrite, const char *header, int value ) {
    char *temp = (char*)CS_tempBuff( 64 );
    snprintf( temp, 64, "%d", value );
    return PrivateSetReplyHeader( reply, overwrite, header, temp );
}

static bool PrivateSetReplyCookie( struct CS_Reply *reply, const char *cookie, const char *value ) {
    if( cookie == NULL || value == NULL ) {
        CS_LOG_ERROR("Trying to set an invalid value as a cookie... %s=%s",cookie?cookie:"NULL",value?value:"NULL");
        return true;
    }
    if( reply->numCookies >= MAX_REPLY_COOKIES ) {
        CS_LOG_ERROR("Trying to add more cookies than we have room for. Max %d", MAX_REPLY_COOKIES);
        return true;
    }
    int nLen = strlen( cookie );
    if( nLen > COOKIE_MAX ) {
        CS_LOG_ERROR("Trying to set a cookie name longer than %d", COOKIE_MAX );
        return true;
    }
    nLen = strlen( value );
    if( nLen > COOKIE_VALUE_MAX ) {
        CS_LOG_ERROR("Trying to set a cookie value longer than %d", COOKIE_VALUE_MAX );
        return true;
    }
    strncpy( reply->setCookie[ reply->numCookies ].cookie, cookie, COOKIE_MAX );
    strncpy( reply->setCookie[ reply->numCookies ].value, value, COOKIE_VALUE_MAX );
    reply->numCookies++;
    return false;
}

struct CS_Reply *CS_serverCreateReply(struct CS_ClientInfo *info, int responseEnum, int mimeEnum, void *outputBuffer, int outputLength ) {
        struct CS_Reply *returnValue = CS_slabTake(info->server->replyStack);
    returnValue->returnStatusEnum = responseEnum;
    returnValue->contentTypeEnum = mimeEnum;
    returnValue->numHeaders = 0;
    returnValue->outputBuffer = outputBuffer;
    returnValue->outputLength = outputLength;
    return returnValue;
}

void CS_serverReturnReply(struct CS_ClientInfo *info, struct CS_Reply *reply) {
    CS_slabReturn(info->server->replyStack, reply);
}

bool CS_serverSetReplyHeader( struct CS_Reply *reply, const char *header, const char *value ) {
    return PrivateSetReplyHeader(reply,true,header,value);
}
bool CS_serverSetReplyHeaderIfMissing( struct CS_Reply *reply, const char *header, const char *value ) {
    return PrivateSetReplyHeader(reply,false,header,value);
}
bool CS_serverSetReplyHeaderInt( struct CS_Reply *reply, const char *header, int value ) {
    return PrivateSetReplyHeaderInt( reply, true, header, value );
}
bool CS_serverSetReplyHeaderIntIfMissing( struct CS_Reply *reply, const char *header, int value ) {
    return PrivateSetReplyHeaderInt( reply, false, header, value );
}
bool CS_serverSetReplyCookie( struct CS_Reply *reply, const char *cookie, const char *value ) {
    return PrivateSetReplyCookie( reply, cookie, value );
}

bool CS_serverDoReply( struct CS_ClientInfo *info, struct CS_Reply *reply ) {
    if( info == NULL || reply == NULL || reply->returnStatusEnum < 0 || reply->returnStatusEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) {
        CS_LOG_ERROR("Bad arguments.");
        return true;
    }
    int replyNumber = CS_httpResponseEnumToCode( reply->returnStatusEnum );
    const char *replyString = CS_httpResponseEnumToString( reply->returnStatusEnum );
    if( reply->contentTypeEnum != CS_MIME_DO_NOT_SET ) {
        CS_serverSetReplyHeaderIfMissing(reply, "Content-Type", CS_mimeEnumToString( reply->contentTypeEnum ) );
    }
    if( reply->outputBuffer != NULL ) {
        CS_serverSetReplyHeaderIntIfMissing(reply, "Content-Length", reply->outputLength );
    }
    CS_serverSetReplyHeaderIfMissing(reply, "Date", timeString(time(NULL)));
    CS_serverSetReplyHeaderIfMissing(reply, "Cache-Control", "no-cache" );
    CS_PP_printf( info->output, "%s %d %s\r\n", HTTP_VERSION, replyNumber, replyString );
    for( int i = 0; i < reply->numHeaders; ++i ) {
        CS_PP_printf( info->output, "%s: %s\r\n", reply->replyHeaders[i].header, reply->replyHeaders[i].value );
    }
    for( int i = 0; i < reply->numCookies; ++i ) {
        CS_PP_printf( info->output, "Set-Cookie: %s=%s", reply->setCookie[i].cookie, reply->setCookie[i].value);
        if( reply->setCookie[i].httpOnly ) CS_PP_printf( info->output, "; HttpOnly" );
        if( info->ssl ) CS_PP_printf( info->output, "; Secure");
        CS_PP_printf( info->output, "\r\n" );
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
        //And stuff out the last two bytes.
        CS_PP_printf( info->output, "\r\n" );
        bytesWritten = info->ssl?
            CS_PP_writeToSSL( info->output, info->ssl ):
            CS_PP_writeToFile( info->output, info->clientSocket );
    }
    CS_serverReturnReply(info, reply);
    return false;
}

