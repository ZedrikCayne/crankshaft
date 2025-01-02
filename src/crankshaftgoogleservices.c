#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftjson.h"
#include "crankshaftpushpull.h"
#include "crankshaftstring.h"

#include "crankshaftstringbuilder.h"
#include "crankshaftgoogleservices.h"
#include "crankshafthttp.h"
#include "crankshaftbase64.h"

struct CS_GoogleKey {
    const char *keyId;
    EVP_PKEY *pKey;
};

int nGoogleKeys = 0;
static struct CS_GoogleKey *googleKeychain = NULL;

static void privateFreeKeys(void) {
    if( googleKeychain ) {
        for( int i = 0; i < nGoogleKeys; ++i ) {
            CS_stringFree( googleKeychain[ i ].keyId );
            googleKeychain[ i ].keyId = NULL;
            EVP_PKEY_free( googleKeychain[ i ].pKey );
            googleKeychain[ i ].pKey = NULL;
        }
        CS_free( googleKeychain );
    }
}

static void privateAllocKeys(int nKeys) {
    privateFreeKeys();
    nGoogleKeys = nKeys;
    googleKeychain = CS_allocZero( nKeys * sizeof( struct CS_GoogleKey ) );
}

static void privateAddKey( const char *kid, EVP_PKEY *pKey ) {
    for( int i = 0; i < nGoogleKeys; ++i ) {
        if( googleKeychain[ i ].pKey == NULL ) {
            googleKeychain[ i ].keyId = CS_stringCopy( kid );
            googleKeychain[ i ].pKey = pKey;
            break;
        }
    }
}

static EVP_PKEY *privateGetKey( const char *kid ) {
    for( int i = 0; i < nGoogleKeys; ++i ) {
        if( googleKeychain[ i ].keyId ) {
            if(strcmp(kid,googleKeychain[i].keyId) == 0) return googleKeychain[i].pKey;
        }
    }
    return NULL;
}

static const char *googleKeysEndpoint = "https://www.googleapis.com/oauth2/v3/certs";

static struct CS_JsonNode *googleServicesJson = NULL;
static const char *googleClientId = NULL;
static const char *googleClientSecret = NULL;
static struct CS_JsonNode *googleKeys = NULL;
static time_t cachedUntil = 0;

static pthread_mutex_t googleServicesMutex = PTHREAD_MUTEX_INITIALIZER;
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
        googleServicesJson = CS_jsonParseCopy( varValue, -1, SECRET_ALLOC_SIZE );
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
        googleServicesJson = CS_jsonParseCopy( json, -1, SECRET_ALLOC_SIZE );
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

#define DEFAULT_KEY_SIZE 8192

static bool privateFetchKeys() {
    if( googleKeys == NULL || cachedUntil < time(NULL) ) {
        if( googleKeychain ) CS_free( googleKeychain );
        pthread_mutex_lock( &googleServicesMutex );
        if( googleKeys == NULL || cachedUntil < time(NULL) ) {
            struct CS_RequestReply *reply = 
                CS_httpMakeRequest( CS_HTTP_METHOD_GET,
                                    googleKeysEndpoint,
                                    NULL, 0,
                                    NULL, 0,
                                    NULL, 0,
                                    NULL );
            if( reply == NULL ) goto UNLOCK_MUTEX_ERROR;
            googleKeys = CS_jsonParseCopy( CS_PP_startOfData(reply->buffer),
                                           CS_PP_dataSize(reply->buffer),
                                           DEFAULT_KEY_SIZE );
            if( googleKeys == NULL ) return true;
            if( CS_jsonNodeToUnquoted( googleKeys, true ) == NULL ) {
                CS_LOG_ERROR("Failed to unquote the reply");
                CS_jsonFree(googleKeys);
                goto UNLOCK_MUTEX_ERROR;
            }
            CS_httpCloseRequest( reply );
            int nKeys = 0;
            struct CS_JsonNode *keysContainer = CS_jsonNodeByPath( googleKeys, "keys" );
            if( keysContainer ) nKeys = keysContainer->nItemsOrLength;
            privateAllocKeys( nKeys );
            CS_JSON_NODE_ITER( keysContainer, keyNode ) {
                struct CS_JsonNode *n = CS_jsonNodeByPath( keyNode, "n" );
                if( !n ) break;
                struct CS_JsonNode *e = CS_jsonNodeByPath( keyNode, "e" );
                if( !e ) break;
                struct CS_JsonNode *kid = CS_jsonNodeByPath( keyNode, "kid" );
                if( !kid ) break;
                int nSize;
                void *nBits = CS_base64DecodeUrlTemp( n->stringValue, n->nItemsOrLength, &nSize );
                if( !nBits ) break;
                int eSize;
                void *eBits = CS_base64DecodeUrlTemp( e->stringValue, e->nItemsOrLength, &eSize );
                if( !eBits ) break;
                BIGNUM *bn = BN_bin2bn( nBits, nSize, NULL );
                if( !bn ) break;
                BIGNUM *be = BN_bin2bn( eBits, eSize, NULL );
                if( !be ) {
                    BN_free( bn );
                    break;
                }
                OSSL_PARAM_BLD *param_builder = OSSL_PARAM_BLD_new();
                if( param_builder ) {
                    OSSL_PARAM_BLD_push_BN( param_builder, "n", bn );
                    OSSL_PARAM_BLD_push_BN( param_builder, "e", be );
                    OSSL_PARAM *param = OSSL_PARAM_BLD_to_param( param_builder );
                    if( param ) {
                        EVP_PKEY *pkey;
                        EVP_PKEY_CTX *pkey_context = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
                        EVP_PKEY_fromdata( pkey_context, &pkey, EVP_PKEY_PUBLIC_KEY, param );
                        privateAddKey( kid->stringValue, pkey );
                        OSSL_PARAM_free( param );
                    }
                    OSSL_PARAM_BLD_free( param_builder );
                }
            }
            pthread_mutex_unlock( &googleServicesMutex );
        }
    }

    return false;
UNLOCK_MUTEX_ERROR:
    pthread_mutex_unlock( &googleServicesMutex );
    return true;
}

bool CS_GS_verifyJwt( const struct CS_Jwt *jwt ) {
    privateGetKey("a");
    return false;

}

bool CS_GS_getKeys(void) {
    return privateFetchKeys();
}

struct CS_StringBuilder *CS_GS_getKeysDesc(void) {
    return googleKeys?CS_jsonNodePrintable( googleKeys ):NULL;
}

