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
#include <signal.h>
#include <poll.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/slaballoc.h>
#include <crankshaft/mutex.h>
#include <crankshaft/network.h>
#include <crankshaft/socket.h>
#include <crankshaft/ssl.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/util.h>
#include <crankshaft/thread.h>
#include <stdint.h>

struct CS_Socket {
    bool ownSocket;
    int32_t socket;
    int32_t port;
    SSL *ssl;
    bool ipv6;
    void *context;
    union {
        struct sockaddr       sockaddr;
        struct sockaddr_in    sockaddr_in;
        struct sockaddr_in6   sockaddr_in6;
    };
    struct CS_Mutex *selfMutex;
    struct CS_Mutex *inputMutex;
    struct CS_Mutex *outputMutex;

    //Input/output buffers.
    struct CS_PushPullBuffer *buffer;
    struct CS_PushPullBuffer *output;
};

struct autoSocketContext {
    struct CS_Socket *socket;
    int32_t inputBufferSize;
    int32_t outputBufferSize;
    bool (*cycle)(struct CS_Thread *socket, int32_t socketEnum, struct CS_Socket *newSocket );
    bool inputMutex;
    bool outputMutex;
};

static struct CS_SlabAllocator *sockets = NULL;
static struct CS_SlabAllocator *socketContexts = NULL;

static pthread_mutex_t socketsMutex = PTHREAD_MUTEX_INITIALIZER;

struct CS_Socket *CS_socketInit( int32_t socket, int32_t port, SSL *ssl, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex ) {
    CS_PMUTEX_PROTECT_GLOBAL( sockets, &socketsMutex ) {
        sockets = CS_slabInit( "SOCKETS", sizeof( struct CS_Socket ), 64, sizeof( void * ) );
        socketContexts = CS_slabInit( "AUTOACCEPT CONTEXTS", sizeof( struct autoSocketContext ), 64, sizeof( void * ) );
        if( sockets == NULL || socketContexts == NULL ) {
            if( sockets ) CS_slabFree( sockets );
            if( socketContexts ) CS_slabFree( socketContexts );
            pthread_mutex_unlock( &socketsMutex );
            return NULL;
        }
        pthread_mutex_unlock( &socketsMutex );
    }

    struct CS_Socket *returnValue = CS_slabTakeZero( sockets );

    if( !returnValue ) goto ERROR_INIT;

    returnValue->socket = socket;
    returnValue->ssl = ssl;
    returnValue->port = port;
    if( inputBufferSize > 0 ) {
        returnValue->buffer = CS_PP_defaultAlloc( inputBufferSize );
        if( returnValue->buffer == NULL ) goto ERROR_INIT;
    }
    if( outputBufferSize > 0 ) {
        returnValue->output = CS_PP_defaultAlloc( outputBufferSize );
        if( returnValue->buffer == NULL ) goto ERROR_INIT;
    }

    if( inputMutex ) returnValue->inputMutex = CS_mutexTakeNamed("sock input mutex");
    if( inputMutex && returnValue->inputMutex == NULL ) goto ERROR_INIT;
    if( outputMutex ) returnValue->outputMutex = CS_mutexTakeNamed("sock output mutex");
    if( outputMutex && returnValue->outputMutex == NULL ) goto ERROR_INIT;
    
    return returnValue;

ERROR_INIT:
    CS_socketDestroy( returnValue );
    return NULL;
}

struct CS_Socket *CS_socketConnect( char *address, bool noInternalNetworks, int32_t port, bool wantSSL, bool TLSv1, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex ) {
    struct CS_Socket *returnValue = CS_socketInit( -1, port, NULL, inputBufferSize, outputBufferSize, inputMutex, outputMutex );
    if( returnValue == NULL ) return NULL;
    //Lookup address
    struct addrinfo *addrInfos = CS_networkLookupAddress( address, port );
    if( addrInfos == NULL ) return NULL;

    //Cycle the infos
    struct addrinfo *addrInfoIter = addrInfos;
    int32_t connectValue = -1;

    //Set that we 'own the socket' so when we destroy ourselves we will kill it.
    returnValue->ownSocket = true;
    do {
        //Network routable check...if we're on ipv4 and have any of the 3 private ranges we 
        //will want to skip it. (Protect the internal range if we're running from an internal
        //host.

        if( !noInternalNetworks || CS_networkAddressRoutable( addrInfoIter->ai_addr ) ) {
            returnValue->socket = socket(AF_INET, SOCK_STREAM, 0);
            if( returnValue->socket < 0 ) {
                CS_LOG_ERROR("Failed to make an outbound socket.");
                break;
            }

            connectValue = connect( returnValue->socket, addrInfoIter->ai_addr, addrInfoIter->ai_addrlen );
            if( connectValue != 0 ) {
                CS_LOG_ERROR("Error connecting: %s", strerror(errno) );
                close(returnValue->socket);
                returnValue->socket = -1;
                addrInfoIter = addrInfoIter->ai_next;
            }
        } else {
            addrInfoIter = addrInfoIter->ai_next;
        }
    } while( connectValue == -1 && addrInfoIter != NULL );
    CS_networkReleaseAddressInfos( addrInfos );
    addrInfos = NULL;

    if( returnValue->socket < 0 || connectValue != 0 )
        goto CLEANUP_CONNECT;

    returnValue->ssl = NULL;
    if( wantSSL ) {
        returnValue->ssl = CS_sslNew( TLSv1 );
        if( returnValue->ssl == NULL ) goto CLEANUP_CONNECT;
        SSL_set_fd( returnValue->ssl, returnValue->socket );
        if( SSL_connect( returnValue->ssl ) <= 0 ) {
            unsigned long err_code = ERR_get_error();
            char *err_buf = CS_tempBuff( 256 );
            ERR_error_string(err_code, err_buf);
            CS_LOG_ERROR("Failed negotiate SSL. %s", err_buf);
            goto CLEANUP_CONNECT;
        }
    }
    return returnValue;

CLEANUP_CONNECT:
    CS_socketDestroy( returnValue );
    return NULL;
}

bool CS_socketClose( struct CS_Socket *socket ) {
    if( !socket || socket->socket < 0 ) return true;
    close( socket->socket );
    socket->socket = -1;
    return false;
}

bool CS_socketIsClosed( struct CS_Socket *socket ) {
    if( !socket || socket->socket < 0 ) return true;
    return false;
}

bool CS_socketDestroy( struct CS_Socket *socket ) {
    if( !socket ) return true;
    if( socket->ownSocket && socket->socket > 0 ) close( socket->socket );
    if( socket->ownSocket && socket->ssl ) SSL_free( socket->ssl );
    if( socket->inputMutex ) CS_mutexReturn( socket->inputMutex );
    if( socket->outputMutex ) CS_mutexReturn( socket->outputMutex );
    if( socket->buffer ) CS_PP_defaultFree( socket->buffer );
    if( socket->output ) CS_PP_defaultFree( socket->output );
    CS_slabReturn( sockets, socket );
    return false;
}

struct CS_PushPullBuffer *CS_socketLockInputBuffer( struct CS_Socket *socket ) {
    if( socket->inputMutex ) CS_mutexLock( socket->inputMutex );
    return socket->buffer;
}

void CS_socketUnlockInputBuffer( struct CS_Socket *socket ) {
    if( socket->inputMutex ) CS_mutexUnlock( socket->inputMutex );
}

struct CS_PushPullBuffer *CS_socketLockOutputBuffer( struct CS_Socket *socket ) {
    if( socket->outputMutex ) CS_mutexLock( socket->outputMutex );
    return socket->output;
}

void CS_socketUnlockOutputBuffer( struct CS_Socket *socket ) {
    if( socket->outputMutex ) CS_mutexUnlock( socket->outputMutex );
}

int32_t CS_socketEmptyOutputBuffer( struct CS_Socket *socket, bool lock ) {
    if( socket == NULL || socket->socket < 0 ) return -1;
    struct CS_PushPullBuffer *pp = lock?CS_socketLockOutputBuffer( socket ):socket->output;
    if( pp == NULL ) return -1;
    signal(SIGPIPE,SIG_IGN);
    int32_t returnValue = socket->ssl?CS_PP_writeToSSL( pp, socket->ssl ):CS_PP_writeToFile( pp, socket->socket );
    if( returnValue < 0 ) {
        CS_socketClose( socket );
    }
    if( lock ) CS_socketUnlockOutputBuffer( socket );
    return returnValue;
}

int32_t CS_socketFillIncomingBuffer( struct CS_Socket *socket, bool lock ) {
    if( socket == NULL || socket->socket < 0 ) return -1;
    struct CS_PushPullBuffer *pp = lock?CS_socketLockInputBuffer( socket ):socket->buffer;
    if( pp == NULL ) return -1;
    signal(SIGPIPE,SIG_IGN);
    int32_t returnValue = socket->ssl?CS_PP_readFromSSL( pp, socket->ssl ):CS_PP_readFromFile( pp, socket->socket );
    if( returnValue < 0 ) {
        CS_socketClose( socket );
    }
    if( lock ) CS_socketUnlockInputBuffer( socket );
    return returnValue;
}

struct CS_Socket *CS_socketBind(int32_t port, bool ipv6, bool wantSSL) {
    int32_t finalPortNum = 0;
    int32_t listenSocket = ipv6?socket(AF_INET6,SOCK_STREAM,0):socket(AF_INET, SOCK_STREAM,0);
    if( listenSocket < 0 ) {
        CS_LOG_ERROR("Failed to open socket for listening.");
        return NULL;
    }

    int32_t optVal = 1;

    if( setsockopt( listenSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal) ) < 0 ) {
        CS_LOG_ERROR("Failed to set socket options.");
        goto ERR_SOCK;
    }
    
    if( ipv6 ) {
        goto ERR_SOCK;
    } else {
        struct sockaddr_in serverAddress = {0};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddress.sin_port = htons(port);
        if( bind( listenSocket, (struct sockaddr *)&serverAddress,sizeof(serverAddress) ) < 0 ) {
            CS_LOG_ERROR("Failed to bind socket to port %d.", port);
            goto ERR_SOCK;
        }

        struct sockaddr_in serverAddressPostBind = {0};
        socklen_t addrLen = sizeof(serverAddressPostBind);
        if( getsockname( listenSocket, (struct sockaddr *)&serverAddressPostBind, &addrLen ) < 0 ) {
            CS_LOG_ERROR("Failed to get socket address.");
            goto ERR_SOCK;
        }
        finalPortNum = ntohs(serverAddressPostBind.sin_port);
    }

    if( listen(listenSocket,5) < 0 ) {
        CS_LOG_ERROR("Failed to listen.");
        goto ERR_SOCK;
    }

    SSL *newSSL = NULL;

    if( wantSSL ) {
        newSSL = CS_sslNew( false );
    }

    struct CS_Socket *returnValue = CS_socketInit( listenSocket, finalPortNum, newSSL, 0, 0, false, false);
    if( returnValue == NULL ) goto ERR_SSL;
    returnValue->ownSocket = true;

    return returnValue;

ERR_SSL:
    SSL_free( newSSL );

ERR_SOCK:
    close(listenSocket);

    return NULL;
}

struct CS_Socket *CS_socketAccept( struct CS_Socket *boundSocket, bool blocking, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex ) {
    do {
        struct pollfd pollMe = {boundSocket->socket, POLLIN, 0};
        pollMe.revents = 0;
        int32_t pollVal = poll(&pollMe, 1, 500);
        if( pollVal < 0 ) break;
        if( pollVal == 1 && pollMe.revents == POLLIN ) {
            struct sockaddr clientSocketAddress;
            socklen_t addrSize = sizeof(struct sockaddr);
            int32_t newSock = accept(boundSocket->socket, &clientSocketAddress, &addrSize);
            if( newSock < 0 ) {
                CS_LOG_ERROR("Socket failed to accept %s", strerror(errno));
                break;
            } else {
                SSL *newSSL = NULL;
                if( boundSocket->ssl ) {
                    newSSL = CS_sslNew(false);
                    SSL_set_fd( newSSL, newSock );
                    if( SSL_accept( newSSL ) <= 0 ) {
                        CS_LOG_ERROR( "ssl failed to accept" );
                        SSL_free( newSSL );
                        break;
                    }
                }
                struct CS_Socket *returnValue = CS_socketInit( newSock, boundSocket->port, newSSL, inputBufferSize, outputBufferSize, inputMutex, outputMutex );
                if( !returnValue ) {
                    close( newSock );
                    if( newSSL ) SSL_free( newSSL );
                } else {
                    returnValue->ownSocket = true;
                }
                return returnValue;
            }
        }
    } while( blocking && !CS_socketIsClosed( boundSocket ) );
    return NULL;
}

bool cycleWrapper( struct CS_Thread *thread, int32_t threadStateEnum, void *context ) {
    struct autoSocketContext *autoContext = (struct autoSocketContext *)context;
    bool returnValue = ((struct autoSocketContext *)context)->cycle( thread, threadStateEnum, autoContext->socket );
    if( threadStateEnum == CS_THREAD_STOP ) {
        CS_slabReturn( socketContexts, autoContext );
    }
    return returnValue;
}

//Driver for a CS_Thread.
bool autoThreadAccept( struct CS_Thread *thread, int32_t threadStateEnum, void *context ) {
    struct autoSocketContext *socketContext = (struct autoSocketContext *)context;

    if( CS_THREAD_STOP != threadStateEnum ) {
        struct CS_Socket *newSocket = CS_socketAccept( socketContext->socket, false, socketContext->inputBufferSize, socketContext->outputBufferSize, socketContext->inputMutex, socketContext->outputMutex );
        if( newSocket ) {
            //Copy new parameters and socket out.
            struct autoSocketContext *newContext = CS_slabTakeCopy( socketContexts, socketContext );
            newContext->socket = newSocket;
            CS_threadStart( CS_tempBuffSnprintf( 60, "auto-accept for %s", CS_threadName( thread ) ), newContext, cycleWrapper );
        }
    } else {
        CS_slabReturn( socketContexts, socketContext );
    }

    return false;
}

struct CS_Thread *CS_socketAutoAccept( const char *threadName, struct CS_Socket *boundSocket, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex, bool (*cycle)(struct CS_Thread *thread, int32_t threadSateEnum, struct CS_Socket *incoming ) ) {
    struct autoSocketContext *newContext = CS_slabTakeZero( socketContexts );
    if( newContext == NULL ) return NULL;
    newContext->inputBufferSize = inputBufferSize;
    newContext->outputBufferSize = outputBufferSize;
    newContext->inputMutex = inputMutex;
    newContext->outputMutex = outputMutex;
    newContext->cycle = cycle;
    newContext->socket = boundSocket;
    struct CS_Thread *returnValue = CS_threadStart( threadName, newContext, autoThreadAccept );
    if( !returnValue ) CS_slabReturn( socketContexts, newContext );
    return returnValue;
}

void *CS_socketGetContext( struct CS_Socket *socket ) {
    return socket->context;
}

void *CS_socketPutContext( struct CS_Socket *socket, void *context ) {
    void *oldContext = socket->context;
    socket->context = context;
    return oldContext;
}

