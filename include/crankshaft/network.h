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

#ifdef __cplusplus
}
#endif
#endif
