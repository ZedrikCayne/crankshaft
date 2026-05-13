#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/util.h>
#include <crankshaft/network.h>
#include <crankshaft/tempbuff.h>
#include <stdint.h>

struct addrinfo *CS_networkLookupAddress( const char *address, int32_t portNum ) {
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

const char *CS_networkAddressToTempString( struct sockaddr *inputAddr ) {
    if( inputAddr->sa_family == AF_INET ) {
        unsigned char *addr = (unsigned char*)&(((struct sockaddr_in*)inputAddr)->sin_addr.s_addr);
        return CS_tempBuffSnprintf( 64, "%d.%d.%d.%d", (int32_t)addr[0], (int32_t)addr[1], (int32_t)addr[2], (int32_t)addr[3] );
    }
    if( inputAddr->sa_family == AF_INET6 ) {
        unsigned char *addr = ((struct sockaddr_in6*)inputAddr)->sin6_addr.s6_addr;
        return CS_tempBuffSnprintf( 64, "%20X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X::%02X", addr[0], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15] );
    }
    return "BAD SOCKADDR";
}

int32_t CS_networkSockaddrSize( struct sockaddr *addr ) {
    if( addr->sa_family == AF_INET ) return sizeof( struct sockaddr_in );
    if( addr->sa_family == AF_INET6 ) return sizeof( struct sockaddr_in6 );
    return 0;
}

bool CS_networkCopySockaddr( struct sockaddr *to, struct sockaddr *addr ) {
    int32_t size = CS_networkSockaddrSize( addr );
    if( size ) {
        memcpy( to, addr, size );
        return false;
    }
    return true;
}

#define IPV4_ADDRESS_BYTES 4
struct unRoutable_v4 {
    char ipv4[IPV4_ADDRESS_BYTES];
    char mask[IPV4_ADDRESS_BYTES];
};

#define IPV6_ADDRESS_BYTES 16
struct unRoutable_v6 {
    char ipv6[IPV6_ADDRESS_BYTES];
    char mask[IPV6_ADDRESS_BYTES];
};

static struct unRoutable_v4 unroutable_v4[] = {
    { {127,  0,  0,  0}, { 0xFF, 0xFF, 0xFF, 0xFC } }, //Localhost
    { {  0,  0,  0,  0}, { 0xFF, 0xFF, 0xFF, 0xFF } }, //Zero
    { { 10,  0,  0,  0}, { 0xFF, 0x00, 0x00, 0x00 } }, //10.0.0.0 private network.
    { {192,168,  0,  0}, { 0xFF, 0xFF, 0x00, 0x00 } }, //192.168.0.0 private network.
    { {172, 16,  0,  0}, { 0xFF, 0xFF, 0xF0, 0x00 } }, //172.16.0.0 private network.
    { {169,254,  0,  0}, { 0xFF, 0xFF, 0x00, 0x00 } }, //169.254.0.0 link local addresses.
};

static struct unRoutable_v6 unroutable_v6[] = {
    { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
      { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } },//Localhost
    { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
      { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } },//Zero
    { { 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
      { 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },//FC00::/7 - Private Internet
    { { 0xF8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
      { 0xFF, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } } //FE80::/10 - Link Local Addresses
};

bool CS_networkAddressRoutable( struct sockaddr *addr ) {
    int32_t numMatch;
    if( addr->sa_family == AF_INET ) {
        struct sockaddr_in *inAddr = (struct sockaddr_in *)addr;
        unsigned char *bytes = (unsigned char *)&(inAddr->sin_addr.s_addr);
        for( int32_t i = 0; i < CS_ARRAY_SIZE( unroutable_v4 ); ++i ) {
            struct unRoutable_v4 *route = unroutable_v4 + i;
            numMatch = 0;
            for( int32_t b = 0; b < IPV4_ADDRESS_BYTES; ++b ) {
                if( route->ipv4[ b ] == (route->mask[ b ] & bytes[ b ]) ) ++numMatch;
                else break;
            }
            if( numMatch == IPV4_ADDRESS_BYTES ) return false;
        }
        return true;
    }
    if( addr->sa_family == AF_INET6 ) {
        struct sockaddr_in6 *inAddr = (struct sockaddr_in6 *)addr;
        unsigned char *bytes = inAddr->sin6_addr.s6_addr;
        for( int32_t i = 0; i < CS_ARRAY_SIZE( unroutable_v6 ); ++i ) {
            struct unRoutable_v6 *route = unroutable_v6 + i;
            numMatch = 0;
            for( int32_t b = 0; b < IPV6_ADDRESS_BYTES; ++b ) {
                if( route->ipv6[ b ] == (route->mask[ b ] & bytes[ b ]) ) ++numMatch;
                else break;
            }
            if( numMatch == IPV6_ADDRESS_BYTES ) return false;
        }
        return true;
    }
    //If we've not gotten to inet 4 or inet 6...we're something definitely wrong.
    return false;
}


