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
#include <signal.h>

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
#include <crankshaft/slaballoc.h>
#include <crankshaft/ssl.h>
#include <crankshaft/network.h>
#include <crankshaft/base64.h>
#include <crankshaft/compress.h>
#include <stdint.h>

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
        int32_t realYear = rt->tm_year + 1900;
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

static void pipeHandler(int32_t sig) {
    signal(sig,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
}

static struct CS_ClientInfo *createClientInfoWithThread( int32_t socket,
                                                      struct CS_WebServer *server,
                                                      struct sockaddr *clientSocketAddress ) {
    struct CS_ClientInfo *ci = CS_allocZero(sizeof(struct CS_ClientInfo));
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
    ci->allocator = CS_linearInit(8129);
    if( ci->allocator == NULL ) {
        CS_LOG_ERROR( "Out of memory allocating variable storage." );
        goto CLIENT_ERR_STORAGE;
    }
    signal(SIGPIPE,pipeHandler);
    //struct timeval tv;
    //tv.tv_sec = 1;
    //tv.tv_usec = 0;
    //if( setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv) ) < 0 ) {
    //    CS_LOG_ERROR( "Failed to set socket options." );
    //    goto CLIENT_ERR_OUTPUT_BUFF;
    //}
    ci->clientSocket = socket;
    
    CS_networkCopySockaddr( &ci->clientSocketAddress, clientSocketAddress );
    pthread_t newThread;
    pthread_attr_t threadAttr;
    pthread_attr_init( &threadAttr );

    ci->disconnectCallback = NULL;
    ci->persistentData = NULL;
    ci->appData = NULL;
    pthread_attr_setstacksize(&threadAttr, PTHREAD_STACK_MIN * 2 );
    int32_t result = pthread_create( &newThread, &threadAttr, clientThread, ci );
    if( result < 0 ) {
        CS_LOG_ERROR( "Failed to create client thread." );
        goto CLIENT_ERR_PTHREAD;
    }
    pthread_detach( newThread );
    return ci;
CLIENT_ERR_PTHREAD:
    CS_linearFree(ci->allocator);
CLIENT_ERR_STORAGE:
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
    if( clientInfo->server->wantSSL ) { 
        clientInfo->ssl = CS_sslNew(false);
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
        int32_t bytesRead = CS_serverFillIncomingBuffer( clientInfo );
        
        if( bytesRead <= 0 ) {
            break;
        } else {
            if( HTTP_STATE_MACHINE(clientInfo) )
                break;
            //We can get wedged in here if we don't make room
            CS_PP_makeRoom(clientInfo->buffer);
            //Clear our copy of the things.
            CS_linearReset(clientInfo->allocator);
        }
    }
CLIENT_BAIL_NOSSL:
    if( clientInfo->allocator ) {
        CS_linearFree(clientInfo->allocator);
        clientInfo->allocator = NULL;
    }
    if( clientInfo->ssl ) {
        SSL_free( clientInfo->ssl );
        clientInfo->ssl = NULL;
    }
    CS_serverKillClientSocket( clientInfo );
    if( clientInfo->disconnectCallback != NULL ) {
        clientInfo->disconnectCallback( clientInfo );
        clientInfo->disconnectCallback = NULL;
    }
    CS_PP_defaultFree(clientInfo->buffer);
    CS_PP_defaultFree(clientInfo->output);
    CS_free(clientInfo);
    pthread_exit(NULL);
    return NULL;
}

static void freeRoutes(struct CS_WebServer *server) {
    for( int32_t i = 0; i < CS_MAX_HTTP_METHODS; ++i ) {
        if( server->routes[ i ] != NULL )
            CS_free( server->routes[ i ] );
        server->routes[ i ] = NULL;
    }
}

static void *serverThreadProc(void *var) {
    struct CS_WebServer *server = (struct CS_WebServer *)var;
    server->threadRunning = true;
    server->killMe = false;
    while(server->threadRunning && !server->killMe ) {
        struct pollfd pollMe = {server->listenSocket, POLLIN, 0};
        pollMe.revents = 0;
        int32_t pollVal = poll(&pollMe, 1, 500);
        if( pollVal < 0 ) break;
        if( pollVal == 1 && pollMe.revents == POLLIN ) {
            struct sockaddr *clientSocketAddress = CS_tempBuff( 128 );
            socklen_t addrSize = sizeof(struct sockaddr);
            int32_t newSock = accept(server->listenSocket, (struct sockaddr *)clientSocketAddress, &addrSize);
            if( newSock < 0 ) {
                CS_LOG_ERROR("Socket closed, error %s", strerror(errno));
                server->threadRunning = false;
            } else {
                createClientInfoWithThread( newSock, server, clientSocketAddress );
            }
        }
    }
    shutdown( server->listenSocket, SHUT_RDWR );
    close( server->listenSocket );
    freeRoutes( server );
    server->listenSocket = -1;
    server->threadRunning = false;
    pthread_exit( NULL );
    return NULL;
}

static const char ReplyStackName[] = "Reply Stack";

struct CS_WebServer *CS_serverStart(int32_t portNum, 
                                           const char *certFile,
                                           const char *keyFile,
                                           const char *selfSignHostname,
                                           const char *fileServingPath,
                                           const char *fileServingFile,
                                           int32_t fileServingCacheControlMaxAge,
                                           struct CS_Route *routes,
                                           int32_t numberOfRoutes ) {
    struct CS_WebServer *returnValue = CS_allocZero( sizeof( struct CS_WebServer ) );
    if( returnValue == NULL ) {
        CS_LOG_ERROR("Could not allocate enough for a web server.");
        return NULL;
    }

    returnValue->defaultFileServingPath = CS_cstringCopy(fileServingPath);
    returnValue->defaultFileServingFile = CS_cstringCopy(fileServingFile);
    returnValue->killMe = false;
    returnValue->threadRunning = false;
    returnValue->logAccess = NULL;
    returnValue->defaultFileServingCacheControlMaxAge = fileServingCacheControlMaxAge;
    returnValue->behindProxy = false;
    
    int32_t i = 0;
    int32_t j = 0;
    for( i = 0; i < CS_MAX_HTTP_METHODS; ++i ) {
        returnValue->routeNumbers[ i ] = 0;
        returnValue->routes[ i ] = NULL;
    }

    for( i = 0; i < numberOfRoutes; ++i ) {
        int32_t currentIndex = routes[ i ].method;
        if( currentIndex < CS_HTTP_METHOD_ANY || currentIndex >= CS_MAX_HTTP_METHODS ) {
            CS_LOG_ERROR("Method defined in routes for web server is outside of allowed range.");
            goto ERR_ALLOC;
        }
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

    int32_t routeCount[ CS_MAX_HTTP_METHODS ] = {0};
    for( i = 0; i < numberOfRoutes; ++i ) {
        int32_t neededRoute = routes[ i ].method;
        if( neededRoute == CS_HTTP_METHOD_ANY ) {
            for( j = 0; j < CS_MAX_HTTP_METHODS; ++j ) {
                int32_t currentWriteIndex = routeCount[ j ];
                memcpy( returnValue->routes[ j ] + currentWriteIndex, routes + i, sizeof( struct CS_Route) );
                ++routeCount[ j ];
            }
        } else {
            int32_t currentWriteIndex = routeCount[ neededRoute ];

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

    int32_t optVal = 1;

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
        returnValue->wantSSL = true;
    } else {
        returnValue->wantSSL = false;
    }

    int32_t result = pthread_create(&returnValue->serverThread, NULL, serverThreadProc, returnValue);
    if( result < 0 ) {
        CS_LOG_ERROR("Thread failed to create.\n");
        goto ERR_SSL;
    }
    pthread_detach(returnValue->serverThread);

    return returnValue;
ERR_SSL:
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
static void ERR(struct CS_ClientInfo *info, int32_t errorEnum, const char *details) {
    char *tempBuff = CS_tempBuff(STACK_BUFFER_SIZE);
    int32_t error = CS_httpResponseEnumToCode( errorEnum );
    const char *errorString = CS_httpResponseEnumToCstring( errorEnum );

    int32_t contentLength = snprintf(tempBuff, STACK_BUFFER_SIZE, "{\"error\":\"%s\",\"status\":%d,\"details\":\"%s\"}",errorString,error,details?details:errorString);
    struct CS_Reply *reply = CS_serverCreateReply( info, errorEnum, CS_MIME_JSON, tempBuff, contentLength );
    if( reply == NULL ) return;
    CS_serverDoReply( info, reply );
}

#define TEMP_OUTPUT_BUFF_SIZE 8192 
static bool BASIC_OK(struct CS_ClientInfo *info, const struct CS_String *what) {
    struct CS_PushPullBuffer tempToWriteBase;
    struct CS_PushPullBuffer *tempToWrite = &tempToWriteBase;
    CS_PP_init(tempToWrite,TEMP_OUTPUT_BUFF_SIZE,CS_tempBuff(TEMP_OUTPUT_BUFF_SIZE));
    struct CS_StringBuilder *scratch = CS_SB_create( TEMP_OUTPUT_BUFF_SIZE );
    if( tempToWrite == NULL )
        return true;
    CS_PP_printf( tempToWrite, "{\"what\":\"%.*s\",\"status\":%d,\"headers\":[",what->length,what->data,200);
    for( int32_t i = 0; i < info->requestInfo.numHeaders; ++i ) { 
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_jsonQuoteStringToStringBuilder(info->requestInfo.headers[i].values.data,info->requestInfo.headers[i].values.length,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%.*s\",\"value\":\"%s\"}",
                info->requestInfo.headers[i].header.length,
                info->requestInfo.headers[i].header.data, scratch->buffer );
    }
    CS_PP_printf( tempToWrite, "],\"queryParameters\":[" );
    for( int32_t i = 0; i < info->requestInfo.numParameters; ++i ) {
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_jsonQuoteStringToStringBuilder(info->requestInfo.parameters[i].value.data,info->requestInfo.parameters[i].value.length,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%.*s\",\"value\":\"%s\"}",
                info->requestInfo.parameters[i].name.length,
                info->requestInfo.parameters[i].name.data, scratch->buffer );
    }
    CS_SB_reset( scratch );
    CS_jsonQuoteStringToStringBuilder(info->requestInfo.uri.data,info->requestInfo.uri.length,scratch);
    CS_PP_printf( tempToWrite, "],\"dataLeftInBuffer\":%d,\"uri\":\"%s\",\"formParameters\":[", CS_PP_dataSize( info->buffer ), scratch->buffer );
    for( int32_t i = 0; i < info->requestInfo.numFormParameters; ++i ) {
        if( i != 0 ) CS_PP_printf( tempToWrite, "," );
        CS_SB_reset( scratch );
        CS_jsonQuoteStringToStringBuilder(info->requestInfo.formParameters[i].value.data,info->requestInfo.formParameters[i].value.length,scratch);
        CS_PP_printf( tempToWrite, "{\"name\":\"%.*s\",\"value\":\"%s\"}",
                info->requestInfo.formParameters[i].name.length,
                info->requestInfo.formParameters[i].name.data, scratch->buffer );
    }
    CS_PP_printf( tempToWrite, "]}" );
    CS_SB_free( scratch );
    struct CS_Reply *reply = CS_serverCreateReply( info, CS_RESPONSE_200, CS_MIME_JSON, CS_PP_startOfData( tempToWrite ), CS_PP_dataSize( tempToWrite ) );
    return CS_serverDoReply( info, reply );
}


static int32_t privateParseForm( struct CS_ClientInfo *info, const char *currentPoint, const char *endOfContent ) {
    const struct CS_String *wholeForm = CS_stringTempReferenceCstring( currentPoint, endOfContent - currentPoint );
    const struct CS_String *formEntry = NULL;
    const char *outerSavePtr = NULL;
    const char *innerSavePtr = NULL;

    while( (formEntry = CS_stringTempStrtok(wholeForm, &CS_STRING("&"), &outerSavePtr)) ) {
        innerSavePtr = NULL;
        struct CS_String *formEntryName = CS_stringTempStrtok(formEntry,&CS_STRING("="),&innerSavePtr);
        struct CS_String *formEntryValue = CS_stringTempStrtok(formEntry,&CS_STRING("="),&innerSavePtr);
        if( formEntryName == NULL || formEntryValue == NULL ) return -1;
        CS_stringInitReference( &info->requestInfo.formParameters[ info->requestInfo.numFormParameters ].name, formEntryName );
        CS_stringInitCopy( &info->requestInfo.formParameters[ info->requestInfo.numFormParameters ].value, formEntryValue );
        CS_httpUrlDecodeInPlace( &info->requestInfo.formParameters[ info->requestInfo.numFormParameters ].value );
        info->requestInfo.numFormParameters++;
    }
    return endOfContent - currentPoint;
}

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
#define SET_STRING(_WHAT) CS_stringInitLinearCopyCstring(&(_WHAT),startOfToken,currentPoint - startOfToken,info->allocator);
#define SET_STRING_COPY(_WHAT) CS_stringInitLinearCopyCstring(&(_WHAT),startOfToken,currentPoint - startOfToken,info->allocator);

static void clearRequestInfo(struct CS_ClientInfo *info) {
    //We used _COPY on these, so we need to actually free them.
    CS_stringFree(&info->requestInfo.uri);
    for( int32_t i = 0; i < info->requestInfo.numHeaders; ++i ) {
        CS_stringFree(&info->requestInfo.headers[i].values);
    }
    for( int32_t i = 0; i < info->requestInfo.numFormParameters; ++i ) {
        CS_stringFree(&info->requestInfo.formParameters[i].value);
    }
    memset(&(info->requestInfo),0,sizeof(struct CS_RequestInfo));
}

static int32_t parseRequest(struct CS_ClientInfo *info) {
    clearRequestInfo(info);
    char *startOfData = CS_PP_startOfData(info->buffer);

    char *currentPoint = startOfData;
    char *startOfToken = NULL;
    char *endOfData = CS_PP_endOfData(info->buffer);
    int32_t currentHeaderState = HEADER_STATE_POSSIBLE_WHITE_SPACE;
    int32_t currentHeaderIndex = 0;
    int32_t currentParameterIndex = 0;
    const struct CS_String *contentType = NULL;
    const struct CS_String *contentLength = NULL;
    const char *endOfContent = NULL;
    int32_t length = 0;

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
                    SET_STRING(info->requestInfo.method);
                    info->requestInfo.requestMethodEnum = CS_httpStringToMethodEnum(&info->requestInfo.method);
                    if( info->requestInfo.requestMethodEnum == CS_HTTP_METHOD_UNKNOWN ) {
                        return -1;
                    }
                    currentHeaderState = HEADER_STATE_URI;
                    startOfToken = NULL;
                }
                break;
            case HEADER_STATE_URI:
                if( *currentPoint == SP || *currentPoint == QUESTION )  {
                    currentHeaderState = (*currentPoint==SP)?HEADER_STATE_HTTP:HEADER_STATE_QUERY_PARAM;
                    SET_STRING_COPY(info->requestInfo.uri);
                    if( info->requestInfo.uri.data[0] != '/' ) {
                        return -1;
                    }
                    if( CS_httpUrlDecodeInPlace( &info->requestInfo.uri ) ) return -1;
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
                    SET_STRING(info->requestInfo.parameters[ currentParameterIndex ].name);
                    startOfToken = NULL;
                    break;
                }
                break;
            case HEADER_STATE_QUERY_PARAM_VALUE:
                if( *currentPoint == AMPERSAND || *currentPoint == POUND || *currentPoint == SP ) {
                    SET_STRING(info->requestInfo.parameters[ currentParameterIndex ].value);
                    ++currentParameterIndex;
                    info->requestInfo.numParameters = currentParameterIndex;
                    startOfToken = NULL;
                    currentHeaderState = (*currentPoint==AMPERSAND)
                        ?HEADER_STATE_QUERY_PARAM
                        :(*currentPoint==POUND)
                            ?HEADER_STATE_FRAGMENT
                            :HEADER_STATE_HTTP;
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
                    contentType = CS_serverGetRequestHeader( info, &CS_STRING("Content-Type") );
                    if( contentType == NULL || CS_stringStrcmp( contentType, &CS_STRING("application/x-www-form-urlencoded") ) != 0 ) {
                        return currentPoint - startOfData;
                    }
                    contentLength = CS_serverGetRequestHeader( info, &CS_STRING("Content-Length") );
                    if( contentLength == NULL ) {
                        return currentPoint - startOfData;
                    }
                    length = CS_stringAtol( contentLength );
                    if( length == 0 ) {
                        return currentPoint - startOfData;
                    }
                    endOfContent = startOfToken + length;
                    //We might not have read anything beyond the header...keep pulling stuff in
                    //until we can't anymore.
                    while( endOfContent > endOfData ) {
                        if( CS_PP_bufferRemaining( info->buffer ) <= 0 ) {
                            return -1;
                        }
                        int32_t bytesRead = CS_serverFillIncomingBuffer( info );
                        if( bytesRead < 0 ) return -1;
                        endOfData = CS_PP_endOfData(info->buffer);
                    }
                    CS_LOG_TRACE( "%.*s", length, startOfToken );
                    if( privateParseForm( info, startOfToken, endOfContent ) < 0 )
                        return -1;
                    return endOfContent - startOfData;
                    break;
                }
                if( currentHeaderIndex >= MAX_REQUEST_HEADERS ) return -1;
                currentHeaderState = HEADER_STATE_HEADER_NAME;
            case HEADER_STATE_HEADER_NAME:
                if( *currentPoint == COLON ) {
                    SET_STRING(info->requestInfo.headers[ currentHeaderIndex ].header);
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
                                SET_STRING(info->requestInfo.headers[ currentHeaderIndex ].values);
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
            case HEADER_STATE_DONE:
                break;
        }
    }

    return -1;
}

static bool HTTP_STATE_MACHINE(struct CS_ClientInfo *info) {
    //Copy off the start (in case of malformed)
    info->lastSize = CS_PP_dataSize( info->buffer );
    info->tempLast = CS_tempMemCopy( CS_PP_startOfData( info->buffer ), CS_PP_dataSize( info->buffer ) );
    CS_LOG_TRACE("%.*s\n", info->lastSize, (char*)info->tempLast );
    int32_t bytesRequiredForHeaders = parseRequest(info);
    
    if( bytesRequiredForHeaders < 0 ) {
        int32_t outputLength;
        char *malformedEncode = CS_base64EncodeTemp( info->tempLast, info->lastSize, &outputLength );
        CS_logfilePrintf( info->server->logAccess, "Malformed: %s %d |%s|", CS_stringTempCstring(CS_networkAddressToTempString( &info->clientSocketAddress ) ), info->lastSize, malformedEncode);
        return true;
    }
    //Consume the bytes for the headers. Might cause the incoming buffer to reset
    //But that should be just fine at this point. Save the # of bytes in case we
    //want to retrieve the whole header again. (Buffer is fully intact at this
    //point)
    info->requestInfo.headerSize = bytesRequiredForHeaders;
    CS_PP_write(info->buffer,bytesRequiredForHeaders);
    if( info->server->logAccess ) {
        CS_logfilePrintf( info->server->logAccess, "Request: %s %s %s", CS_stringTempCstring(CS_networkAddressToTempString( &info->clientSocketAddress ) ), CS_stringTempCstring(&info->requestInfo.method), CS_stringTempCstring(&info->requestInfo.uri) );
    }
    int32_t requestEnum = info->requestInfo.requestMethodEnum;
    int32_t nRoutes = info->server->routeNumbers[ requestEnum ];
    struct CS_Route *routes = info->server->routes[ requestEnum ];

    for( int32_t i = 0; i < nRoutes; ++i ) {
        switch( routes[ i ].routeType ) {
            case CS_ROUTE_TYPE_FILTER:
                info->appData = routes[i].appData;
                if( routes[i].handler(info) ) {
                    return true;
                }
                break;
            case CS_ROUTE_TYPE_WILDCARD:
                info->appData = routes[i].appData;
                return routes[ i ].handler( info );
                break;
            case CS_ROUTE_TYPE_PREFIX:
                info->appData = routes[i].appData;
                if( CS_stringStrncmp(&info->requestInfo.uri,routes[ i ].route, routes[i].route->length) == 0 )
                    return routes[ i ].handler( info );
                break;
            case CS_ROUTE_TYPE_EXACT:
                info->appData = routes[i].appData;
                if( CS_stringStrcmp( &info->requestInfo.uri, routes[ i ].route ) == 0 )
                    return routes[ i ].handler( info );
                break;
        }
    }
    
    ERR(info, CS_RESPONSE_404, "URI not available on this server");
    return true;
}

bool CS_serverDiagnostic200( struct CS_ClientInfo *info ) {
    return BASIC_OK(info, &info->requestInfo.method);
}

bool CS_serverReplyError( struct CS_ClientInfo *info, int32_t responseEnum, const char *details ) {
    ERR( info, responseEnum, details );
    return true;
}

#define MAX_FILE_PATH 2048
bool CS_serverPushFile( const char *fileToOpen, struct CS_ClientInfo *info, int32_t cacheSeconds, struct CS_Reply *useMe ) {
    const char *extension;
    const char *retryFile = fileToOpen;
    int32_t len = 0;

PUSH_FILE_RETRY:

    len = strlen(retryFile);
    for( int32_t i = len - 2; i > 1; --i ) {
        if( retryFile[ i ] == '.' ) {
            extension = retryFile + i + 1; break;
        }
    } 

    int32_t inputFile = open( retryFile, O_RDONLY );
    if( inputFile < 0 ) {
        ERR(info, CS_RESPONSE_404,"File Not Found");
        goto ERR_SETUP;
    }

    struct stat statBuff;
    if( fstat( inputFile, &statBuff ) < 0 ) {
        ERR(info, CS_RESPONSE_500,"Cannot stat file.");
        goto ERR_FILE_OPENED;
    }

    //Did we open a directory?
    if((statBuff.st_mode & S_IFMT) == S_IFDIR) {
        close( inputFile );
        retryFile = CS_tempBuffSnprintf( MAX_FILE_PATH, "%s/index.html", fileToOpen );
        goto PUSH_FILE_RETRY;
    }
    
    long fileSize = statBuff.st_size;
    long lastModified = statBuff.st_mtime;

    void *fileBuffer = NULL;

    struct CS_RequestInfo *request = &info->requestInfo;
    if( request->requestMethodEnum != CS_HTTP_METHOD_HEAD ) {
        fileBuffer = CS_alloc(fileSize);
        if(fileBuffer == NULL) {
            char *tBuff = CS_tempBuff(256);
            snprintf(tBuff, 256, "OOM allocating %ld bytes for a file.", fileSize);
            ERR(info, CS_RESPONSE_500, tBuff );
            goto ERR_FILE_OPENED;
        }

        int32_t numBytesRead = 1;
        int32_t numBytesToRead = fileSize;
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
    const char *mimeType = CS_mimeFileExtensionToCstring(extension);

    //If we're using the one we got, we need to put the output buffer in manually so
    //the do-send will do it's thing.
    struct CS_Reply *reply = useMe?useMe:CS_serverCreateReply( info, CS_RESPONSE_200, CS_MIME_DO_NOT_SET, fileBuffer, fileSize );
    if( useMe ) {
        reply->outputBuffer = fileBuffer;
        reply->outputLength = fileSize;
    }
    
    CS_serverSetReplyHeader(reply, &CS_STRING("Last-Modified"), &CS_STRING_PTR(timeString( lastModified) ) );
    CS_serverSetReplyHeader(reply, &CS_STRING("Content-Type"), &CS_STRING_PTR(mimeType) );
    reply->closeConnection = true;
    if( cacheSeconds != 0 ) {
        CS_serverSetReplyHeader(reply, &CS_STRING("Cache-Control"), CS_stringTempSnprintf(64,"max-age=%d", cacheSeconds ) );
    } else {
        CS_serverSetReplyHeader(reply, &CS_STRING("Cache-Control"), &CS_STRING("no-cache") );
    }
    CS_serverDoReply(info,reply);
ERR_BUFF_FAILED:
    CS_free(fileBuffer);
ERR_FILE_OPENED:
    close( inputFile );
ERR_SETUP:
    return true;
}

bool CS_serverFileServer( struct CS_ClientInfo *info ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    int32_t lengthOfUri = request->uri.length;
    char *tempBuff = (char*)CS_stringTempCstring(&request->uri);
    char *path = tempBuff;
    const char *filename = NULL;
    //Check for anyone being sneaky about ..
    for( int32_t i = 1; i < lengthOfUri - 1; ++i ) {
        if( tempBuff[i] == '.' && tempBuff[ i - 1 ] == '.' ) {
            ERR(info,CS_RESPONSE_403,"Relative paths not allowed.");
            goto ERR_SETUP;
        }
    }
    if( lengthOfUri > 1 ) {
        for( int32_t i = lengthOfUri - 1; i >= 0; --i ) {
            if( tempBuff[i] == '/' ) {
                tempBuff[i] = 0;
                if( i < lengthOfUri - 1 ) filename = tempBuff + i + 1;
                break;
            }
        }
    }

    char *fileToOpen = CS_tempBuff(MAX_FILE_PATH);

    int32_t printed = snprintf( fileToOpen, MAX_FILE_PATH, "%s%s/%s", info->server->defaultFileServingPath, path, filename?filename:info->server->defaultFileServingFile );
    if( printed >= MAX_FILE_PATH ) {
        ERR(info, CS_RESPONSE_403,"Requested file path length too long.");
        goto ERR_SETUP;
    }

    CS_serverPushFile( fileToOpen, info, info->server->defaultFileServingCacheControlMaxAge, NULL );

ERR_SETUP:
    return true;
}

void CS_serverRemoveRequestHeader( struct CS_ClientInfo *info, const struct CS_String *header ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int32_t i = 0; i < request->numHeaders; ++i ) {
        if( CS_stringStrncmp(header,&request->headers[i].header,HEADER_MAX) == 0 ) {
            if( i <= request->numHeaders - 1 ) {
                memcpy(&request->headers[i],&request->headers[i + 1],sizeof(struct CS_RequestHeader) * (request->numHeaders - i));
            }
            request->numHeaders--;
        }
    }
}

const struct CS_String *CS_serverGetRequestHeader( struct CS_ClientInfo *info, const struct CS_String *header ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int32_t i = 0; i < request->numHeaders; ++i ) {
        if( CS_stringStrncmp(header,&request->headers[i].header,HEADER_MAX) == 0 ) {
            return &request->headers[i].values;
        }
    }
    return NULL;
}

const struct CS_String *CS_serverGetRequestQueryParameter( struct CS_ClientInfo *info, const struct CS_String *name ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int32_t i = 0; i < request->numHeaders; ++i ) {
        if( CS_stringStrncmp(name,&request->parameters[i].name,HEADER_MAX) == 0 ) {
            return &request->parameters[i].value;
        }
    }
    return NULL;
}

const struct CS_String *CS_serverGetRequestCookie( struct CS_ClientInfo *info, const struct CS_String *cookie ) {
    if( cookie == NULL ) return NULL;
    const struct CS_String *cookieValue = CS_serverGetRequestHeader( info, &CS_STRING("Cookie") );
    if( cookieValue == NULL ) return NULL;
    const char *savePtrOuter = NULL;
    const char *savePtrInner = NULL;
    struct CS_String *current = NULL;
    struct CS_String *innerCurrent = NULL;
    struct CS_String *innerValue = NULL;
    while( (current = CS_stringTempStrtok( cookieValue, &CS_STRING(";"), &savePtrOuter )) ) {
        savePtrInner = NULL;
        innerCurrent = CS_stringTempStrtok( current, &CS_STRING("="), &savePtrInner );
        innerValue = CS_stringTempStrtok( current, &CS_STRING("="), &savePtrInner );
        CS_stringLtrim( innerCurrent );
        if( CS_stringStrcmp( cookie, innerCurrent ) == 0 ) {
            return innerValue;
        }
    }
    return NULL;
}

const struct CS_String *CS_serverGetRequestFormParameter( struct CS_ClientInfo *info, const struct CS_String *name ) {
    struct CS_RequestInfo *request = &info->requestInfo;
    for( int32_t i = 0; i < request->numFormParameters; ++i ) {
        if( CS_stringStrncmp(name,&request->formParameters[i].name,HEADER_MAX) == 0 ) {
            return &request->formParameters[i].value;
        }
    }
    return NULL;
}

static bool PrivateSetReplyHeader( struct CS_Reply *reply,
                                   bool overwrite,
                                   const struct CS_String *header, 
                                   const struct CS_String *value ) {
    if( header == NULL || value == NULL ) {
        CS_LOG_ERROR("Trying to set a reply header with a null header or value.");
        return true;
    }
    if( header->length > HEADER_MAX - 1 ) {
        CS_LOG_ERROR("Trying to set a reply header longer than %d", HEADER_MAX-1);
        return true;
    }
    if( value->length > HEADER_VALUE_MAX - 1 ) {
        CS_LOG_ERROR("Trying to set a value in a reply header longer than %d", HEADER_VALUE_MAX-1); 
        return true;
    }

    int32_t indexAlreadySet = -1;
    int32_t indexToSet = reply->numHeaders;

    for( int32_t i = 0; i < reply->numHeaders; ++i ) {
        if( CS_stringStrncmp(header, &reply->replyHeaders[i].name.name, HEADER_MAX) == 0) {
            indexAlreadySet = i;
            indexToSet = i;
            break;
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
            CS_stringCopyToStatic(&reply->replyHeaders[indexToSet].name.name, header, HEADER_MAX);
        }
        CS_stringCopyToStatic(&reply->replyHeaders[indexToSet].value.value, value, HEADER_VALUE_MAX);
        return false;
    }
    return true;
}

static bool PrivateSetReplyHeaderInt( struct CS_Reply *reply, bool overwrite, const struct CS_String *header, int32_t value ) {
    return PrivateSetReplyHeader( reply, overwrite, header, CS_stringTempSnprintf(64, "%d", value ) );
}

static bool PrivateSetReplyCookie( struct CS_Reply *reply, const struct CS_String *cookie, const struct CS_String *value, bool httpOnly, int32_t sameSiteEnum ) {
    if( cookie == NULL || value == NULL ) {
        CS_LOG_ERROR("Trying to set an invalid value as a cookie... %s=%s",CS_stringTempCstringOrNULL(cookie),CS_stringTempCstringOrNULL(value) );
        return true;
    }
    if( reply->numCookies >= MAX_REPLY_COOKIES ) {
        CS_LOG_ERROR("Trying to add more cookies than we have room for. Max %d", MAX_REPLY_COOKIES);
        return true;
    }
    if( sameSiteEnum < CS_REPLY_COOKIE_SAMESITE_LAX || sameSiteEnum > CS_REPLY_COOKIE_SAMESITE_NONE ) {
        CS_LOG_ERROR("Trying to set the SameSite attribute on a cookie out of range.");
        return true;
    }
    if( cookie->length > COOKIE_MAX ) {
        CS_LOG_ERROR("Trying to set a cookie name longer than %d", COOKIE_MAX - 1 );
        return true;
    }
    if( value->length > COOKIE_VALUE_MAX ) {
        CS_LOG_ERROR("Trying to set a cookie value longer than %d", COOKIE_VALUE_MAX - 1 );
        return true;
    }
    CS_stringCopyToStatic( &reply->setCookie[ reply->numCookies ].cookie.cookie, cookie, COOKIE_MAX );
    CS_stringCopyToStatic( &reply->setCookie[ reply->numCookies ].value.value, value, COOKIE_MAX );
    reply->setCookie[ reply->numCookies ].httpOnly = httpOnly;
    reply->setCookie[ reply->numCookies ].sameSiteEnum = sameSiteEnum;
    reply->numCookies++;
    return false;
}

const struct CS_String *PrivateGetReplyHeader(const struct CS_Reply *reply, const struct CS_String *header ) {
    if( (reply == NULL) || (header == NULL) || (header->length == 0) ) {
        return NULL;
    }
    for(int32_t i = 0; i < reply->numHeaders; ++i) {
        if(CS_stringStrncasecmp(header, &reply->replyHeaders[i].name.name, HEADER_MAX) == 0) {
            return &reply->replyHeaders[i].value.value;
        }
    }

    return NULL;
}

const struct CS_String *CS_serverGetRequestTempIdAddress( struct CS_ClientInfo *info ) {
    if( info->server->behindProxy ) {
        const struct CS_String *realIp = CS_serverGetRequestHeader( info, &CS_STRING("X-Real-IP") );
        if( realIp ) return realIp;
    }
    return CS_networkAddressToTempString( &info->clientSocketAddress );
}

struct CS_Reply *CS_serverCreateReply(struct CS_ClientInfo *info, int32_t responseEnum, int32_t mimeEnum, const void *outputBuffer, int32_t outputLength ) {
        struct CS_Reply *returnValue = CS_slabTake(info->server->replyStack);
    returnValue->returnStatusEnum = responseEnum;
    returnValue->contentTypeEnum = mimeEnum;
    returnValue->numHeaders = 0;
    returnValue->numCookies = 0;
    returnValue->closeConnection = false;
    returnValue->outputBuffer = outputBuffer;
    returnValue->outputLength = outputLength;
    return returnValue;
}

void CS_serverReturnReply(struct CS_ClientInfo *info, struct CS_Reply *reply) {
    CS_slabReturn(info->server->replyStack, reply);
}

bool CS_serverSetReplyHeader( struct CS_Reply *reply, const struct CS_String *header, const struct CS_String *value ) {
    return PrivateSetReplyHeader(reply,true,header,value);
}
bool CS_serverSetReplyHeaderIfMissing( struct CS_Reply *reply, const struct CS_String *header, const struct CS_String *value ) {
    return PrivateSetReplyHeader(reply,false,header,value);
}
bool CS_serverSetReplyHeaderInt( struct CS_Reply *reply, const struct CS_String *header, int32_t value ) {
    return PrivateSetReplyHeaderInt( reply, true, header, value );
}
bool CS_serverSetReplyHeaderIntIfMissing( struct CS_Reply *reply, const struct CS_String *header, int32_t value ) {
    return PrivateSetReplyHeaderInt( reply, false, header, value );
}
const struct CS_String *CS_serverGetReplyHeader( struct CS_Reply *reply, const struct CS_String *header ) {
    return PrivateGetReplyHeader( reply, header );
}
bool CS_serverSetReplyCookie( struct CS_Reply *reply, const struct CS_String *cookie, const struct CS_String *value, bool httpOnly, int32_t sameSiteEnum ) {
    return PrivateSetReplyCookie( reply, cookie, value, httpOnly, sameSiteEnum );
}

bool CS_serverDoReply( struct CS_ClientInfo *info, struct CS_Reply *reply ) {
    if( info == NULL || reply == NULL || reply->returnStatusEnum < 0 || reply->returnStatusEnum >= MAX_NUM_CS_RESPONSE_ENUMS ) {
        CS_LOG_ERROR("Bad arguments.");
        return true;
    }

    void *compressedBuffer = NULL;
    int32_t compressedLength = 0;
    const struct CS_String *acceptEncoding = CS_serverGetRequestHeader(info, &CS_STRING("Accept-Encoding") );

    if (acceptEncoding && CS_stringStrstr(acceptEncoding, &CS_STRING("gzip") ) && 
        reply->outputBuffer != NULL && reply->outputLength > 128) {
        
        bool shouldCompress = false;
        const struct CS_String *contentType = NULL;
        if (reply->contentTypeEnum != CS_MIME_DO_NOT_SET) {
            contentType = CS_stringTempCopyCstring(CS_mimeEnumToCstring(reply->contentTypeEnum), -1);
        } else {
            contentType = CS_serverGetReplyHeader(reply, &CS_STRING("Content-Type") );
        }

        if (contentType) {
            if (CS_stringStrstr(contentType, &CS_STRING("text/") ) || 
                CS_stringStrstr(contentType, &CS_STRING("application/json") ) ||
                CS_stringStrstr(contentType, &CS_STRING("application/javascript") ) ||
                CS_stringStrstr(contentType, &CS_STRING("application/xml") ) ||
                CS_stringStrstr(contentType, &CS_STRING("image/svg+xml") ) ) {
                shouldCompress = true;
            }
        } else {
            shouldCompress = true; 
        }

        if (shouldCompress) {
            if( CS_serverGetReplyHeader(reply,&CS_STRING("Content-Encoding") ) != NULL ) {
                shouldCompress = false;
            }
        }

        if (shouldCompress) {
            CS_Compress ctx;
            CS_compressInit(&ctx);
            int32_t bufferSize = reply->outputLength + 1024 + (reply->outputLength / 100);
            void *outBuff = CS_alloc(bufferSize);
            if (outBuff) {
                struct CS_PushPullBuffer in_pp, out_pp;
                CS_PP_init(&in_pp, reply->outputLength, (char*)reply->outputBuffer);
                in_pp.currentReadOffset = reply->outputLength;
                CS_PP_init(&out_pp, bufferSize, (char*)outBuff);
                
                if (CS_compressGzip(&ctx, &in_pp, &out_pp) >= 0) {
                    if (CS_compressGzip(&ctx, &in_pp, &out_pp) >= 0) {
                        compressedBuffer = outBuff;
                        compressedLength = CS_PP_dataSize(&out_pp);
                        CS_serverSetReplyHeader(reply, &CS_STRING("Content-Encoding"), &CS_STRING("gzip") );
                    }
                }
                if (!compressedBuffer) CS_free(outBuff);
            }
            CS_compressDestroy(&ctx);
        }
    }

    int32_t replyNumber = CS_httpResponseEnumToCode( reply->returnStatusEnum );
    const char *replyString = CS_httpResponseEnumToCstring( reply->returnStatusEnum );
    if( reply->contentTypeEnum != CS_MIME_DO_NOT_SET ) {
        CS_serverSetReplyHeaderIfMissing(reply, &CS_STRING("Content-Type"), CS_stringTempCopyCstring( CS_mimeEnumToCstring( reply->contentTypeEnum), -1 ) );
    }

    void *bufferToSend = compressedBuffer ? compressedBuffer : (void*)reply->outputBuffer;
    int32_t lengthToSend = compressedBuffer ? compressedLength : reply->outputLength;

    CS_serverSetReplyHeaderInt(reply, &CS_STRING("Content-Length"), lengthToSend );

    CS_serverSetReplyHeaderIfMissing(reply, &CS_STRING("Date"), &CS_STRING_PTR(timeString(time(NULL))));
    CS_serverSetReplyHeaderIfMissing(reply, &CS_STRING("Cache-Control"), &CS_STRING("no-cache") );
    if( reply->closeConnection ) {
        CS_serverSetReplyHeaderIfMissing(reply, &CS_STRING("Connection"), &CS_STRING("close") );
    }
    CS_PP_printf( info->output, "%s %d %s\r\n", HTTP_VERSION, replyNumber, replyString );
    for( int32_t i = 0; i < reply->numHeaders; ++i ) {
        CS_PP_printf( info->output, "%s: %s\r\n", 
                CS_stringTempCstring(&reply->replyHeaders[i].name.name),
                CS_stringTempCstring(&reply->replyHeaders[i].value.value) );
    }
    for( int32_t i = 0; i < reply->numCookies; ++i ) {
        CS_PP_printf( info->output, "Set-Cookie: %s=%s",
                CS_stringTempCstring(&reply->setCookie[i].cookie.cookie),
                CS_stringTempCstring(&reply->setCookie[i].value.value) );
        if( reply->setCookie[i].httpOnly ) CS_PP_printf( info->output, "; HttpOnly" );
        if( info->ssl ) CS_PP_printf( info->output, "; Secure");
        switch( reply->setCookie[i].sameSiteEnum ) {
            case CS_REPLY_COOKIE_SAMESITE_LAX:
                break;
            case CS_REPLY_COOKIE_SAMESITE_STRICT:
                CS_PP_printf( info->output, "; SameSite=Strict" );
                break;
            case CS_REPLY_COOKIE_SAMESITE_NONE:
                CS_PP_printf( info->output, "; SameSite=None" );
                break;
        }
        CS_PP_printf( info->output, "; Path=/");
        CS_PP_printf( info->output, "\r\n" );
    }
    CS_PP_printf( info->output, "\r\n" );

    CS_LOG_TRACE( "%.*s", CS_PP_dataSize( info->output ), CS_PP_startOfData( info->output ) );
    int32_t bytesWritten = 0;

    if( bufferToSend != NULL ) {
        int32_t bytesToWrite = lengthToSend + CS_PP_dataSize( info->output );
        int32_t bytesPutInBuff = 0;
        int32_t totalBytesTaken = 0;

        do {
            bytesPutInBuff = CS_PP_readFromBuffer( info->output, ((char*)bufferToSend) + totalBytesTaken, lengthToSend - totalBytesTaken );
            totalBytesTaken += bytesPutInBuff;
            bytesWritten = CS_serverWriteOutputBuffer( info );
            bytesToWrite -= bytesWritten;
        } while( bytesToWrite > 0 && bytesWritten >= 0 );
    }
    //Stuff out everything else.
    while( bytesWritten >= 0 && CS_PP_dataSize( info->output ) > 0 ) {
        bytesWritten = CS_serverWriteOutputBuffer( info );
        if( bytesWritten < 0 )
            break;
    }
    if (compressedBuffer) CS_free(compressedBuffer);
    bool requireClose = reply->closeConnection;
    CS_serverReturnReply(info, reply);
    return requireClose || bytesWritten < 0;
}

int32_t CS_serverKillClientSocket( struct CS_ClientInfo *info ) {
    int32_t returnValue = 0;
    if( info->clientSocket ) {
        fsync( info->clientSocket );
        returnValue = close( info->clientSocket );
    }
    info->clientSocket = 0;
    return returnValue;
}

int32_t CS_serverFillIncomingBuffer( struct CS_ClientInfo *info ) {
    if( info == NULL ) return -1;
    return info->ssl?
        CS_PP_readFromSSL(info->buffer,info->ssl):
        CS_PP_readFromFile(info->buffer,info->clientSocket);
}

int32_t CS_serverWriteOutputBuffer( struct CS_ClientInfo *info ) {
    return info->ssl?
        CS_PP_writeToSSL( info->output, info->ssl ):
        CS_PP_writeToFile( info->output, info->clientSocket );
}
