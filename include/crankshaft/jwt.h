#ifndef __crankshaftjwtdoth__
#define __crankshaftjwtdoth__
#include <stdbool.h>

/********************************************************************
 *
 * Basic JWT decoding. Signature verification example in
 * CS_GS_verifyJwt
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Jwt {
    //Original pieces in base 64
    const char *header;
    const char *payload;
    const char *signature;
    //Decoded pieces
    struct CS_JsonNode *jsonHeader;
    struct CS_JsonNode *jsonPayload;
    const void *signatureInBinary;
    void *linearAllocator;
    int headerLength;
    int payloadLength;
    int signatureLength;
    int binarySignatureLength;
};

const struct CS_Jwt *CS_jwtParse( const char *jwt, int jwtLength, int allocatorSize );
void CS_jwtFree( const struct CS_Jwt *jwt );
bool CS_jwtVerify( const struct CS_Jwt *jwt );

#ifdef __cplusplus
}
#endif
#endif
