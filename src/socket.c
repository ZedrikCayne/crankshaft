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

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/slaballoc.h>
#include <crankshaft/mutex.h>
#include <crankshaft/network.h>
#include <crankshaft/socket.h>
#include <crankshaft/ssl.h>
#include <crankshaft/tempbuff.h>

static struct CS_SlabAllocator *sockets = NULL;

static pthread_mutex_t socketsMutex = PTHREAD_MUTEX_INITIALIZER;

struct CS_Socket *CS_socketInit( int socket, SSL *ssl, int inputBufferSize, int outputBufferSize, bool inputMutex, bool outputMutex ) {
    if( sockets == NULL ) {
        pthread_mutex_lock( &socketsMutex );
        if( sockets == NULL ) {
            sockets = CS_slabInit( "SOCKETS", sizeof( struct CS_Socket ), 64, sizeof( void * ) );
            if( sockets == NULL ) {
                pthread_mutex_unlock( &socketsMutex );
                return NULL;
            }
        }
        pthread_mutex_unlock( &socketsMutex );
    }

    struct CS_Socket *returnValue = CS_slabTakeZero( sockets );

    if( !returnValue ) goto ERROR_INIT;

    returnValue->socket = socket;
    returnValue->ssl = ssl;
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

struct CS_Socket *CS_socketConnect( char *address, bool noInternalNetworks, int port, bool wantSSL, bool TLSv1, int inputBufferSize, int outputBufferSize, bool inputMutex, bool outputMutex ) {
    struct CS_Socket *returnValue = CS_socketInit( -1, NULL, inputBufferSize, outputBufferSize, inputMutex, outputMutex );
    if( returnValue == NULL ) return NULL;
    //Lookup address
    struct addrinfo *addrInfos = CS_networkLookupAddress( address, port );
    if( addrInfos == NULL ) return NULL;

    //Cycle the infos
    struct addrinfo *addrInfoIter = addrInfos;
    int connectValue = -1;

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

int CS_socketEmptyOutputBuffer( struct CS_Socket *socket, bool lock ) {
    if( socket == NULL || socket->socket < 0 ) return -1;
    struct CS_PushPullBuffer *pp = lock?CS_socketLockOutputBuffer( socket ):socket->output;
    if( pp == NULL ) return -1;
    signal(SIGPIPE,SIG_IGN);
    int returnValue = socket->ssl?CS_PP_writeToSSL( pp, socket->ssl ):CS_PP_writeToFile( pp, socket->socket );
    if( returnValue < 0 ) {
        CS_socketClose( socket );
    }
    if( lock ) CS_socketUnlockOutputBuffer( socket );
    return returnValue;
}

int CS_socketFillIncomingBuffer( struct CS_Socket *socket, bool lock ) {
    if( socket == NULL || socket->socket < 0 ) return -1;
    struct CS_PushPullBuffer *pp = lock?CS_socketLockInputBuffer( socket ):socket->buffer;
    if( pp == NULL ) return -1;
    signal(SIGPIPE,SIG_IGN);
    int returnValue = socket->ssl?CS_PP_readFromSSL( pp, socket->ssl ):CS_PP_readFromFile( pp, socket->socket );
    if( returnValue < 0 ) {
        CS_socketClose( socket );
    }
    if( lock ) CS_socketUnlockInputBuffer( socket );
    return returnValue;
}


