#ifndef __crankshaftsocketdoth__
#define __crankshaftsocketdoth__
#include <stdbool.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <crankshaft/pushpull.h>
#include <stdint.h>

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
struct CS_Socket;

struct CS_Socket *CS_socketInit( int32_t socket, int32_t portNum, SSL *ssl, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex );
struct CS_Socket *CS_socketConnect( char *address, bool noInternalNetworks, int32_t port, bool wantSSL, bool TLSv1, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex );
struct CS_Socket *CS_socketBind(int32_t port, bool ipv6, bool wantSSL );
struct CS_Socket *CS_socketAccept( struct CS_Socket *boundSocket, bool blocking, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex );
struct CS_Thread *CS_socketAutoAccept( const char *threadName, struct CS_Socket *boundSocket, int32_t inputBufferSize, int32_t outputBufferSize, bool inputMutex, bool outputMutex, bool (*cycle)(struct CS_Thread *thread, int32_t threadSateEnum, struct CS_Socket *incoming ) );
bool CS_socketDestroy( struct CS_Socket *socket );
bool CS_socketClose( struct CS_Socket *socket );
bool CS_socketIsClosed( struct CS_Socket *socket );
void CS_socketLock( struct CS_Socket *socket );
struct CS_PushPullBuffer *CS_socketLockInputBuffer( struct CS_Socket *socket );
void CS_socketUnlockInputBuffer( struct CS_Socket *socket );
struct CS_PushPullBuffer *CS_socketLockOutputBuffer( struct CS_Socket *socket );
void CS_socketUnlockOutputBuffer( struct CS_Socket *socket );
//Returns -1 on socket write/read fails. Might return 0 if there's no data
//that is ready to go out.
int32_t CS_socketEmptyOutputBuffer( struct CS_Socket *socket, bool lock );
int32_t CS_socketFillIncomingBuffer( struct CS_Socket *socket, bool lock );
void *CS_socketGetContext( struct CS_Socket *socket );
void *CS_socketPutContext( struct CS_Socket *socket, void *context );
void CS_socketTeardownAll();

#ifdef __cplusplus
}
#endif
#endif
