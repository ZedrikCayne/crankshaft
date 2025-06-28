#include <stdlib.h>
#include <stdio.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/network.h>

struct addrinfo *CS_networkLookupAddress( const char *address, int portNum ) {
    struct addrinfo hints = { 0 };
    char portNumString[64];
    snprintf( portNumString, 64, "%d", portNum );
    hints.ai_flags = AI_PASSIVE|AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addrInfos;
    if( getaddrinfo(address,portNum > 0?portNumString:NULL,&hints,&addrInfos) != 0 ) {
        CS_LOG_WARN("CS_httpLookupAddress() Address lookup fail: %s", address);
        return NULL;
    }
    return addrInfos;
}

void CS_networkReleaseAddressInfos( struct addrinfo *infos ) {
    freeaddrinfo( infos );
}

