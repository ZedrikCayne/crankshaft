#ifndef __crankshaftnetworkdoth__
#define __crankshaftnetworkdoth__
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

struct addrinfo *CS_networkLookupAddress( const char *address, int portNum );
void CS_networkReleaseAddressInfos( struct addrinfo *infos );

int CS_networkSockaddrSize( struct sockaddr *addr );
bool CS_networkCopySockaddr( struct sockaddr *to, struct sockaddr *addr );
const char *CS_networkAddressToTempString( struct sockaddr *addr );

bool CS_networkAddressRoutable( struct sockaddr *addr );

#ifdef __cplusplus
}
#endif
#endif
