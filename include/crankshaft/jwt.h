#ifndef __crankshaftjwtdoth__
#define __crankshaftjwtdoth__
#include <stdbool.h>
#include <stdint.h>

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
    int32_t headerLength;
    int32_t payloadLength;
    int32_t signatureLength;
    int32_t binarySignatureLength;
};

const struct CS_Jwt *CS_jwtParse( const char *jwt, int32_t jwtLength, int32_t allocatorSize );
void CS_jwtFree( const struct CS_Jwt *jwt );
bool CS_jwtVerify( const struct CS_Jwt *jwt );

#ifdef __cplusplus
}
#endif
#endif
