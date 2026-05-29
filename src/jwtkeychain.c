#include <stdlib.h>
#include <stdio.h>

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/http.h>
#include <crankshaft/json.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/storage.h>
#include <crankshaft/cache.h>
#include <crankshaft/tempbuff.h>
#include <crankshaft/hashtable.h>
#include <crankshaft/base64.h>
#include <crankshaft/list.h>
#include <crankshaft/string.h>

#include <crankshaft/jwtkeychain.h>
#include <stdint.h>

#define DEFAULT_KEY_SIZE 8192

const struct CS_Cache *webCache = NULL;
struct CS_HashTable *keyIdToEVP_PKEY = NULL;

static struct CS_StorageItem *privateWebCache( const struct CS_Cache *cache, const char *key ) {
    struct CS_String *tempUri = CS_stringTempCopyCstring(key,CS_STRING_USE_STRLEN);
    struct CS_RequestReply *reply = 
        CS_httpMakeRequest( CS_HTTP_METHOD_GET,
                            tempUri,
                            NULL, 0,
                            NULL, 0,
                            NULL, 0,
                            NULL, 0,
                            true, 
                            NULL );
    if( reply == NULL ) return NULL;

    time_t expireTime = 0;

    const struct CS_String *cacheControlHeader = CS_httpReplyHeader( reply, &CS_STRING("cache-control") );
    if( cacheControlHeader != NULL ) {
        const struct CS_String *maxAge = CS_stringStrstr( cacheControlHeader, &CS_STRING("max-age") );
        if( maxAge != NULL ) {
            const char *savePtr;
            const struct CS_String *equals = CS_stringTempStrtok( maxAge, &CS_STRING("="), &savePtr );
            if( equals != NULL ) {
                const struct CS_String *restOfString = CS_stringTempStrtok( NULL, &CS_STRING(","), &savePtr );
                if( restOfString != NULL ) {
                    expireTime = time(NULL) + CS_stringAtoi(restOfString);
                }
            }
        }
    }
    if( expireTime == 0 ) expireTime = time(NULL) + 3600;
    struct CS_PushPullBuffer *whichPP = CS_httpGetReplyBuffer( reply );
    struct CS_StorageItem *returnValue = CS_cachePut( cache, key, CS_PP_startOfData(whichPP), CS_PP_dataSize(whichPP), expireTime );

    CS_httpCloseRequest( reply );
    return returnValue;
}

bool CS_jwtkeychainInit(const struct CS_Storage *backingStorage) {
    if( webCache ) return true;

    webCache = CS_cacheCreate( backingStorage, privateWebCache, NULL, NULL );

    if( !webCache ) return NULL;

    keyIdToEVP_PKEY = CS_HASHTABLE_STRING_VOID( 64, CS_HASHTABLE_FLAG_MUTEX | CS_HASHTABLE_FLAG_VERY_PEDANTIC );

    if( !keyIdToEVP_PKEY ) {
        CS_cacheDestroy( webCache );
        webCache = NULL;
    }

    return webCache == NULL;
}

bool CS_jwtkeychainTeardown() {
    if( !webCache ) return true;
    
    return CS_cacheDestroy( webCache );
}

bool CS_jwtkeychainFetchPublicKeys( const char *urlToFetchKeysFrom ) {
    struct CS_StorageItem *keysJsonItem = CS_cacheGet( webCache, urlToFetchKeysFrom );
    struct CS_JsonNode *keysJson = NULL;

    if( keysJsonItem == NULL ) return true;

    keysJson = CS_jsonParseCopy( keysJsonItem->value,
                                 keysJsonItem->size,
                                 DEFAULT_KEY_SIZE );
    CS_storageReturnItem( keysJsonItem );

    if( keysJson == NULL ) return true;

    bool returnValue = false;

    if( CS_jsonNodeToUnquoted( keysJson, true ) == NULL ) {
        CS_LOG_ERROR("Failed to unquote the reply");
        returnValue = true;
    }

    if( !returnValue ) {
        struct CS_JsonNode *keysContainer = CS_jsonNodeByPath( keysJson, "keys" );
        CS_JSON_NODE_ITER( keysContainer, keyNode ) {
            struct CS_JsonNode *n = CS_jsonNodeByPath( keyNode, "n" );
            if( !n ) break;
            struct CS_JsonNode *e = CS_jsonNodeByPath( keyNode, "e" );
            if( !e ) break;
            struct CS_JsonNode *kid = CS_jsonNodeByPath( keyNode, "kid" );
            if( !kid ) break;
            int32_t nSize;
            void *nBits = CS_base64DecodeUrlTemp( n->stringValue, n->nItemsOrLength, &nSize );
            if( !nBits ) break;
            int32_t eSize;
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
                                const void *oldVal = CS_hashtablePut( keyIdToEVP_PKEY, kid->stringValue, pkey ); 
                                //If the hashtable put returns a value, it is the previous value. We
                                //should delete it.
                                if( oldVal != NULL ) {
                                    EVP_PKEY_free( (EVP_PKEY*)oldVal );
                                }
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
    }

    CS_jsonFree(keysJson);
    return false;
}

EVP_PKEY *CS_jwtkeychainGetKey( const char *keyId ) {
    const void *key = NULL;
    if( keyIdToEVP_PKEY ) key = CS_hashtableGet( keyIdToEVP_PKEY, keyId ); 
    if( key == CS_HASHTABLE_ERROR ) {
        key = NULL;
    }
    return (EVP_PKEY*)key;
}

struct CS_List *CS_jwtkeychainGetKeyIds() {
    return CS_hashtableGetKeys( keyIdToEVP_PKEY );
}

struct CS_StringBuilder *CS_jwtkeychainGetKeyDesc(void) {
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

