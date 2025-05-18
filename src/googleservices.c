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

#include <crankshaft/stringbuilder.h>
#include <crankshaft/googleservices.h>
#include <crankshaft/http.h>
#include <crankshaft/base64.h>

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
            ++nGoogleKeys;
            break;
        }
    }
}

static bool privateFetchKeys();
static EVP_PKEY *privateGetKey( const char *kid ) {
    privateFetchKeys();
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

#define DEFAULT_KEY_SIZE 8192

static bool privateFetchKeys() {
    if( googleKeys == NULL || cachedUntil < time(NULL) ) {
        if( googleKeychain ) CS_free( googleKeychain );
        if( OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS|
                             OPENSSL_INIT_ADD_ALL_CIPHERS|
                             OPENSSL_INIT_ADD_ALL_DIGESTS|
                             OPENSSL_INIT_LOAD_CONFIG,
                             NULL) != 1 ) {
            CS_LOG_ERROR("Openssl init fail.");
            return true;
        }

        pthread_mutex_lock( &googleServicesMutex );
        if( googleKeys == NULL || cachedUntil < time(NULL) ) {
            struct CS_PushPullBuffer *cached = CS_PP_fromFile( "googleKeysCache.json" );
            if( !cached ) {
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
                CS_httpCloseRequest( reply );
                if( googleKeys == NULL ) return true;
            } else {
                googleKeys = CS_jsonParseCopy( CS_PP_startOfData( cached ), CS_PP_dataSize( cached ), DEFAULT_KEY_SIZE );
            }
            if( CS_jsonNodeToUnquoted( googleKeys, true ) == NULL ) {
                CS_LOG_ERROR("Failed to unquote the reply");
                CS_jsonFree(googleKeys);
                goto UNLOCK_MUTEX_ERROR;
            }

            if( !cached ) {
                CS_LOG_INFO("Print request inner.");
                char * temp = CS_jsonNodePrintableTemp( googleKeys );
                FILE *ftemp = fopen( "googleKeysCache.json", "w" );
                if( ftemp == NULL ) CS_LOG_ERROR("ABOUT TO BLOW UP");
                fprintf( ftemp, "%s", temp );
                fclose( ftemp );
            }

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
                    if( OSSL_PARAM_BLD_push_BN( param_builder, OSSL_PKEY_PARAM_RSA_N, bn ) != 1 ) {
                        CS_LOG_ERROR("Failed to push n onto the param builder.");
                        break;
                    }
                    if( OSSL_PARAM_BLD_push_BN( param_builder, OSSL_PKEY_PARAM_RSA_E, be ) != 1 ) {
                        CS_LOG_ERROR("Failed to push e onto the param builder.");
                        break;
                    }
                    OSSL_PARAM *param = OSSL_PARAM_BLD_to_param( param_builder );
                    if( param ) {
                        EVP_PKEY_CTX *pkey_context = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);

                        if( pkey_context != NULL ) {
                            if( EVP_PKEY_fromdata_init( pkey_context ) != 1 ) {
                                CS_LOG_ERROR("Failed fromdata init.");
                            } else {
                                EVP_PKEY *pkey = NULL;
                                if( EVP_PKEY_fromdata( pkey_context, &pkey, EVP_PKEY_PUBLIC_KEY, param ) == 1 ) {
                                    privateAddKey( kid->stringValue, pkey );
                                } else {
                                    CS_LOG_ERROR("Failed to create a pkey from a context.");
                                }
                            }
                            EVP_PKEY_CTX_free( pkey_context );
                        } else {
                            CS_LOG_ERROR("Failed to create a pkey context.");
                        }
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
        CS_LOG_ERROR("googleservices: No private key for %s", keyIdNode->stringValue );
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
    return privateFetchKeys();
}

struct CS_StringBuilder *CS_GS_getKeysDesc(void) {
    return googleKeys?CS_jsonNodePrintable( googleKeys ):NULL;
}

