#ifndef __crankshaftnetworkdoth__
#define __crankshaftnetworkdoth__
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>

#include <crankshaft/string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct addrinfo *CS_networkLookupAddress( const char *address, int32_t portNum );
void CS_networkReleaseAddressInfos( struct addrinfo *infos );

int32_t CS_networkSockaddrSize( struct sockaddr *addr );
bool CS_networkCopySockaddr( struct sockaddr *to, struct sockaddr *addr );
const struct CS_String *CS_networkAddressToTempString( struct sockaddr *addr );

bool CS_networkAddressRoutable( struct sockaddr *addr );

#ifdef __cplusplus
}
#endif
#endif
