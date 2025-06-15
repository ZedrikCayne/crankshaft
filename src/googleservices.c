#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/json.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/string.h>
#include <crankshaft/storage.h>
#include <crankshaft/jwtkeychain.h>

#include <crankshaft/stringbuilder.h>
#include <crankshaft/googleservices.h>
#include <crankshaft/http.h>
#include <crankshaft/base64.h>

static const char *googleKeysEndpoint = "https://www.googleapis.com/oauth2/v3/certs";

static struct CS_JsonNode *googleServicesJson = NULL;
static const char *googleClientId = NULL;
static const char *googleClientSecret = NULL;

#define SECRET_ALLOC_SIZE 2048

static void privateClearSecret(void) {
    if( googleServicesJson ) {
        CS_jsonFree( googleServicesJson );
        googleClientId = NULL;
        googleClientSecret = NULL;
    }
}

static void privateParseSecret(void) {
    if( googleServicesJson ) {
        struct CS_JsonNode *node;
        node = CS_jsonNodeByPath( googleServicesJson, "web/client_id" );
        googleClientId = node?node->stringValue:NULL;
        node = CS_jsonNodeByPath( googleServicesJson, "web/client_secret" );
        googleClientSecret = node?node->stringValue:NULL;
    }
}


bool CS_GS_initWithFile( const char * pathToGoogleServicesJson ) {
    bool returnValue = true;
    privateClearSecret();
    FILE *jsFile = fopen( pathToGoogleServicesJson, "r" );
    if( jsFile ) {
        struct CS_PushPullBuffer *pp = CS_PP_defaultAlloc( SECRET_ALLOC_SIZE );
        if( pp ) {
            if( CS_PP_readFromFILE( pp, jsFile ) > 0 ) {
                googleServicesJson = CS_jsonParseCopy( CS_PP_startOfData(pp), CS_PP_dataSize(pp), SECRET_ALLOC_SIZE );
                if( googleServicesJson )
                    returnValue = false;
            }
            CS_PP_defaultFree( pp );
        }
        fclose( jsFile );
    }
    privateParseSecret();
    return returnValue;
}

bool CS_GS_initWithEnvironmentVariable( const char *variableName ) {
    bool returnValue = true;
    privateClearSecret();
    const char * varValue = getenv( variableName ) ;
    if( varValue ) {
        int nLen = strlen( varValue );
        googleServicesJson = CS_jsonParseCopy( varValue, nLen, SECRET_ALLOC_SIZE );
        if( googleServicesJson )
            returnValue = false;
    }
    privateParseSecret();
    return returnValue;
}

bool CS_GS_initWithEmbeddedJson( const char *json ) {
    bool returnValue = true;
    privateClearSecret();
    if( json ) {
        int nLen = strlen( json );
        googleServicesJson = CS_jsonParseCopy( json, nLen, SECRET_ALLOC_SIZE );
        if( googleServicesJson )
            returnValue = false;
    }
    privateParseSecret();
    return returnValue;
}

bool CS_GS_kill( void ) {
    privateClearSecret();
    return false;
}

const char *CS_GS_getClientID() {
    return googleClientId;
}


EVP_PKEY *privateGetKey( const char *key ) {
    EVP_PKEY *returnValue = CS_jwtkeychainGetKey( key );

    if( returnValue == NULL ) {
        if( !CS_jwtkeychainFetchPublicKeys( googleKeysEndpoint ) ) {
            returnValue = CS_jwtkeychainGetKey( key );
        }
    }
    
    return returnValue;
}

bool CS_GS_verifyJwt( const struct CS_Jwt *jwt ) {
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
    EVP_PKEY *pkey = privateGetKey( keyIdNode->stringValue );
    if( !pkey ) {
        struct CS_StringBuilder *sb = CS_GS_getKeysDesc();
        CS_LOG_ERROR("googleservices: No private key for %s, we have keys for %s", keyIdNode->stringValue, CS_SB_buffer( sb ) );
        CS_SB_free( sb );
        
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
        CS_LOG_ERROR("googleservices: Cannot create a message digest context.");
        goto FAIL_FREE_SB;
    }
    EVP_PKEY_CTX *newCtx;
    if( EVP_DigestVerifyInit(mdctx, &newCtx, EVP_sha256(), NULL, pkey ) != 1 ) {
        CS_LOG_ERROR("googleservices: Could not create message digest verifier.");
        goto FAIL_FREE_MD_CTX;
    }
    if( EVP_DigestVerify( mdctx, jwt->signatureInBinary, jwt->binarySignatureLength, (unsigned char *)CS_SB_buffer(sb), CS_SB_size(sb) ) != 1 ) {
        CS_LOG_ERROR("googleservices: Digest verify fail.");
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

bool CS_GS_getKeys(void) {
    return CS_jwtkeychainFetchPublicKeys( googleKeysEndpoint );
}

struct CS_StringBuilder *CS_GS_getKeysDesc(void) {
    struct CS_List *keys = CS_jwtkeychainGetKeyIds();
    if( keys == NULL ) return NULL;
    struct CS_StringBuilder *sb = CS_SB_create(1024);
    bool addComma = false;
    CS_LIST_ITER(keys, listItem) {
        if( listItem->size > 0 ) {
            if( addComma ) CS_SB_appendChar( sb, ',' );
            CS_SB_append( sb, (char*)listItem->what );
            addComma = true;
        }
    }
    CS_listDestroy( keys );
    return sb;
}

