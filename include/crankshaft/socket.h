#ifndef __crankshaftsocketdoth__
#define __crankshaftsocketdoth__
#include <stdbool.h>
#include <pthread.h>
#include <openssl/ssl.h>

#include <crankshaft/pushpull.h>
#include <crankshaft/socket.h>

#ifdef __cplusplus
extern "C" {
#endif


/********************************************************************
 *
 * CS_Socket: Wrapper for sockets with io buffers.
 * Very opinionated. The structure of the whole system assumes
 * every set of io systems are running on a single thread. This
 * socket can implement a mutex wrapper for input/output in case
 * the user needs one.
 *
 * CS_socketInit allows you to provide your own SSL and socket.
 *
 * CS_socketConnect does all the connect/negotiation given an
 *                  
 *
 ********************************************************************/
struct CS_Socket {
    bool ownSocket;
    int socket;
    SSL *ssl;
    struct CS_Mutex *inputMutex;
    struct CS_Mutex *outputMutex;

    //Input/output buffers.
    struct CS_PushPullBuffer *buffer;
    struct CS_PushPullBuffer *output;
};

struct CS_Socket *CS_socketInit( int socket, SSL *ssl, int inputBufferSize, int outputBufferSize, bool inputMutex, bool outputMutex );
struct CS_Socket *CS_socketConnect( char *address, bool noInternalNetworks, int port, bool wantSSL, bool TLSv1, int inputBufferSize, int outputBufferSize, bool inputMutex, bool outputMutex );
bool CS_socketDestroy( struct CS_Socket *socket );
bool CS_socketClose( struct CS_Socket *socket );
bool CS_socketIsClosed( struct CS_Socket *socket );
struct CS_PushPullBuffer *CS_socketLockInputBuffer( struct CS_Socket *socket );
void CS_socketUnlockInputBuffer( struct CS_Socket *socket );
struct CS_PushPullBuffer *CS_socketLockOutputBuffer( struct CS_Socket *socket );
void CS_socketUnlockOutputBuffer( struct CS_Socket *socket );
//Returns -1 on socket write/read fails. Might return 0 if there's no data
//that is ready to go out.
int CS_socketEmptyOutputBuffer( struct CS_Socket *socket, bool lock );
int CS_socketFillIncomingBuffer( struct CS_Socket *socket, bool lock );

#ifdef __cplusplus
}
#endif
#endif
