#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/base64.h>
#include <crankshaft/linearalloc.h>
#include <crankshaft/json.h>

#include <crankshaft/jwt.h>
#include <crankshaft/jwtkeychain.h>
#include <stdint.h>

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
//    int32_t headerLength;
//    int32_t payloadLength;
//    int32_t signatureLength;
//    int32_t binarySignatureLength;
//};

enum {
    PARSE_HEADER,
    PARSE_PAYLOAD,
    PARSE_SIGNATURE,
    PARSE_END
};

const struct CS_Jwt *CS_jwtParse( const char *inJwt,
       int32_t jwtLength,
       int32_t linearAllocatorSize ) {
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
    int32_t parseState = PARSE_HEADER;
    outJwt->header = NULL;
    outJwt->payload = NULL;
    outJwt->signature = NULL;
    while( in < inEnd ) {
        if( startOfCurrentWebTokenBit == NULL )
            startOfCurrentWebTokenBit = in;
        if( *in == '.' || *in == 0 || in == (inEnd-1) ) {
            int32_t nLen = in - startOfCurrentWebTokenBit;
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
    int32_t tempJsonStringLength;
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

bool CS_jwtVerify( const struct CS_Jwt *jwt ) {
    bool returnValue = false;
    if( !jwt ) {
        CS_LOG_ERROR("googlesercices: Provided jwt is NULL");
        goto FAIL;
    }
    struct CS_JsonNode *keyIdNode = CS_jsonNodeByPath( jwt->jsonHeader, "kid" );
    if( !keyIdNode ) {
        CS_LOG_ERROR("googleservices: Provided jwt does not have a 'kid' field.");
        goto FAIL;
    }
    EVP_PKEY *pkey = CS_jwtkeychainGetKey( keyIdNode->stringValue );
    if( !pkey ) {
        CS_LOG_ERROR("googleservices: Missing key for jwt.");
        goto FAIL;
    }
    struct CS_StringBuilder *sb = CS_SB_create( 1024 );
    if( !sb ) {
        CS_LOG_ERROR("Failed to create a string buffer.");
        goto FAIL;
    }
    CS_SB_append( sb, jwt->header );
    CS_SB_appendChar( sb,'.' );
    CS_SB_append( sb, jwt->payload );

    EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
    if( !mdctx ) {
        CS_LOG_ERROR("jwt: Cannot create a message digest context.");
        goto FAIL_FREE_SB;
    }
    EVP_PKEY_CTX *newCtx;
    if( EVP_DigestVerifyInit(mdctx, &newCtx, EVP_sha256(), NULL, pkey ) != 1 ) {
        CS_LOG_ERROR("jwt: Could not create message digest verifier.");
        goto FAIL_FREE_MD_CTX;
    }
    if( EVP_DigestVerify( mdctx, jwt->signatureInBinary, jwt->binarySignatureLength, (unsigned char *)CS_SB_buffer(sb), CS_SB_size(sb) ) != 1 ) {
        CS_LOG_ERROR("jwt: Digest verify fail.");
        goto FAIL_FREE_MD_CTX;
    }
    returnValue = true;

FAIL_FREE_MD_CTX:
    EVP_MD_CTX_free( mdctx );
FAIL_FREE_SB:
    CS_SB_free(sb);
FAIL:
    return returnValue;
}


