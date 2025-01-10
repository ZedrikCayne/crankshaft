#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/base64.h>
#include <crankshaft/linearalloc.h>
#include <crankshaft/json.h>

#include <crankshaft/jwt.h>

//struct CS_Jwt {
//    //Original pieces in base 64
//    const char *header;
//    const char *payload;
//    const char *signature;
//    //Decoded pieces
//    const struct CS_JsonNode *jsonHeader;
//    const struct CS_JsonNode *jsonPayload;
//    const void *signatureInBinary;
//    void *linearAllocator;
//    int headerLength;
//    int payloadLength;
//    int signatureLength;
//    int binarySignatureLength;
//};

enum {
    PARSE_HEADER,
    PARSE_PAYLOAD,
    PARSE_SIGNATURE,
    PARSE_END
};

const struct CS_Jwt *CS_jwtParse( const char *inJwt,
       int jwtLength,
       int linearAllocatorSize ) {
    void *linearAllocator = CS_linearInit( linearAllocatorSize );
    if( !linearAllocator ) return NULL;
    struct CS_Jwt *outJwt = CS_linearTakeZero( linearAllocator, sizeof( struct CS_Jwt ), sizeof(void*) );
    if( outJwt == NULL ) {
        CS_LOG_ERROR( "CS_jwtParse: initial linear allocator size way too small. Should be al least %d", jwtLength );
        CS_linearFree( linearAllocator );
        return NULL;
    }
    outJwt->linearAllocator = linearAllocator;
    const char *in = inJwt;
    const char *inEnd = inJwt + jwtLength;
    const char *startOfCurrentWebTokenBit = NULL;
    int parseState = PARSE_HEADER;
    outJwt->header = NULL;
    outJwt->payload = NULL;
    outJwt->signature = NULL;
    while( in < inEnd ) {
        if( startOfCurrentWebTokenBit == NULL )
            startOfCurrentWebTokenBit = in;
        if( *in == '.' || *in == 0 || in == (inEnd-1) ) {
            int nLen = in - startOfCurrentWebTokenBit;
            if( in == inEnd-1 && *in != 0 ) nLen += 1;
            char * out = CS_linearTakeZero( linearAllocator, nLen + 1, sizeof(void*) );
            if( out == NULL ) goto ERROR;
            memcpy( out, startOfCurrentWebTokenBit, nLen );
            out[ nLen ] = 0;
            if( !out ) goto ERROR;
            switch( parseState ) {
                case PARSE_HEADER:
                    outJwt->header = out;
                    outJwt->headerLength = nLen;
                    break;
                case PARSE_PAYLOAD:
                    outJwt->payload = out;
                    outJwt->payloadLength = nLen;
                    break;
                case PARSE_SIGNATURE:
                    outJwt->signature = out;
                    outJwt->signatureLength = nLen;
                    break;
            }
            ++parseState;
            startOfCurrentWebTokenBit = NULL;
            if( parseState == PARSE_END )
                break;
        }
        ++in;
    }
    if( parseState < PARSE_PAYLOAD ) {
        CS_LOG_ERROR("JWT: Needs at least a header and payload.");
        goto ERROR;
    }
    int tempJsonStringLength;
    char *tempJsonString = CS_base64DecodeUrlTemp( outJwt->header, outJwt->headerLength, &tempJsonStringLength );
    if( tempJsonString == NULL ) goto ERROR;
    outJwt->jsonHeader = CS_jsonParseCopyWithAllocator( tempJsonString, tempJsonStringLength, linearAllocator );
    if( outJwt->jsonHeader == NULL ) goto ERROR;
    CS_jsonNodeToUnquoted( outJwt->jsonHeader, true );
    tempJsonString = CS_base64DecodeUrlTemp( outJwt->payload, outJwt->payloadLength, &tempJsonStringLength );
    if( tempJsonString == NULL ) goto ERROR;
    outJwt->jsonPayload = CS_jsonParseCopyWithAllocator( tempJsonString, tempJsonStringLength, linearAllocator );
    if( outJwt->jsonPayload == NULL ) goto ERROR;
    CS_jsonNodeToUnquoted( outJwt->jsonPayload, true );
    if( outJwt->signature ) {
        outJwt->signatureInBinary = CS_base64DecodeUrlLinearAlloc( outJwt->signature, outJwt->signatureLength, &tempJsonStringLength, linearAllocator );
        if( outJwt->signatureInBinary == NULL ) goto ERROR;
        outJwt->binarySignatureLength = tempJsonStringLength;
    }
    return outJwt;
ERROR:
    if( outJwt ) CS_jwtFree( outJwt );
    return NULL;
}

void CS_jwtFree( const struct CS_Jwt *jwt ) {
    if( jwt ) {
        if( jwt->jsonPayload ) CS_jsonFree( (struct CS_JsonNode *)jwt->jsonPayload );
        if( jwt->jsonHeader ) CS_jsonFree( (struct CS_JsonNode *)jwt->jsonHeader );
        CS_linearFree( jwt->linearAllocator );
    }
}

