#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshaftjson.h"
#include "crankshaftbase64.h"

#include "crankshaftjwt.h"

extern bool test_jwt(void);

static int testCount = 0;
static int testSucceeded = 0;

static const char *testJwt = "eyJhbGciOiJSUzI1NiIsImtpZCI6ImFiODYxNGZmNjI4OTNiYWRjZTVhYTc5YTc3MDNiNTk2NjY1ZDI0NzgiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL2FjY291bnRzLmdvb2dsZS5jb20iLCJhenAiOiI5NjMyMTU5NzI5MjEtOXZsYmV0bjA0cGVicTRjZHNudWI1cHZlczIwcHFwOXEuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJhdWQiOiI5NjMyMTU5NzI5MjEtOXZsYmV0bjA0cGVicTRjZHNudWI1cHZlczIwcHFwOXEuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJzdWIiOiIxMTYwMjQ2MTQ3MDk4MDA2MTQxOTAiLCJlbWFpbCI6InplZHJpa2NheW5lQGdtYWlsLmNvbSIsImVtYWlsX3ZlcmlmaWVkIjp0cnVlLCJuYmYiOjE3MzUzMjcyNTMsIm5hbWUiOiJaZWRyaWsgQ2F5bmUiLCJwaWN0dXJlIjoiaHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnRlbnQuY29tL2EvQUNnOG9jS3I3cWRmM0RkdGNha3NIQW96bV9LeWJhQjBnNnRiNUFzQl9uU1d2d1lBemNzZ0MyWGM1dz1zOTYtYyIsImdpdmVuX25hbWUiOiJaZWRyaWsiLCJmYW1pbHlfbmFtZSI6IkNheW5lIiwiaWF0IjoxNzM1MzI3NTUzLCJleHAiOjE3MzUzMzExNTMsImp0aSI6Ijk3N2Q2NTg0ZWYyYjdhZTliYjViNmFlYTVmZTdlZmNjNDhhNWFiODEifQ.eFq2kPgAT7JmWZCv5wyNkpXaF_QLfCArCSYBdm1zC8VIu6Qn4GDQwZxxDx3bTAnbD8V7S_3DH9s9q22OSXJe5yaN6GhqlB8c8EiJfZ0otR2ybrTXKc1vUDRqEQLxTyMxSroxizNwDpIrYLpCpD8wPahaKwUVhGk5s8ClKsV0xChTXBV8W5KsQ10MU-5IPpERHDUUKOPO0tLWYAvJGaUv9IKLBxtgV2e0NESiI81WAGHY6zmUmt39wZ-gbKFqbeCv_tHxkGJN2FKe7iFMlg7qIX8L7FnyO2PqtDyPS6uai5u6RIuzbFPFt39ZluwZxZ-Qw2rXAjD99Nk7k6_Mr9d1KQ";


static const char *testKeys = "{ \"keys\": [ { \"kid\": \"31b8fccb2e52253b133138acf0e56632f09957ee\", \"n\": \"qL80q4yfbwG9vt_x1CBgv51oMOlOV1nEIWxcPrEJ_hd1Zf6Tv-gGNQUTzdRqhWUB7VZbIe7IGQ8XrqqZhJkSSRutWYgcB7CZAQPsz2uUzJfULIrqU5-3s1V6TsAvDd0XAhTrxsukBZhvSUcObns6oyr2tvCeYkdlbZ7HHgUjGLt2JduwfVfSDVOXm9iev36W0cDv2RS45H7c4rBaXnvhMQ6BOMA8xeSI05SCwYGpZp5prgQE_xyBPB_EHBOheDOgdtOEvceGg4zMSRni7a5S0ux5EmTOUVxMXOdOzlLmBHMNPYpAjjgYz4afhNuwAlp0BhEhSWwINOGh22U8iU1pIQ\", \"e\": \"AQAB\", \"use\": \"sig\", \"kty\": \"RSA\", \"alg\": \"RS256\" }, { \"use\": \"sig\", \"e\": \"AQAB\", \"alg\": \"RS256\", \"kty\": \"RSA\", \"kid\": \"ab8614ff62893badce5aa79a7703b596665d2478\", \"n\": \"t9OfDNXi2-_bK3_uZizLHS8j8L-Ef4jHjhFvCBbKHkOPOrHQFVoLTSl2e32lIUtxohODogPoYwJKu9uwzpKsMmMj2L2wUwzLB3nxO8M-gOLhIriDWawHMobj3a2ZbVz2eILpjFShU6Ld5f3mQfTV0oHKA_8QnkVfoHsYnexBApJ5xgijiN5BtuK2VPkDLR95XbSnzq604bufWJ3YPSqy8Qc8Y_cFPNtyElePJk9TD2cbnZVpNRUzE7dW9gUtYHFFRrv0jNSKk3XZ-zzkTpz-HqxoNnnyD1c6QK_Ge0tsfsIKdNurRE6Eyuehq9hw-HrI1qdCz-mIqlObQiGdGWx0tQ\" } ] }";
bool test_jwt(void) {
    //Tests go here:
    const struct CS_Jwt *jwt = CS_jwtParse( testJwt, strlen(testJwt), 8192 );

    struct CS_JsonNode *keysJson = CS_jsonParseCopy( testKeys, strlen(testKeys), 1024 );

    CS_FAIL_ON_NULL( jwt, "Create JWT.", "Failed." );

    if( jwt ) {
        if( keysJson ) {
            struct CS_JsonNode *keyIdNode = CS_jsonNodeByPath( jwt->jsonHeader, "kid" );
            if( keyIdNode ) {
                struct CS_JsonNode *keyNode = CS_jsonNodeByPath( keysJson, CS_tempBuffSnprintf( 128, "keys/|kid=%s", keyIdNode->stringValue ) );
                CS_FAIL_ON_NULL( keyNode, "Grab the key out of the keys.", "Failed." );
                if( keyNode ) {
                    struct CS_JsonNode *n = CS_jsonNodeByPath( keyNode, "n" );
                    struct CS_JsonNode *e = CS_jsonNodeByPath( keyNode, "e" );
                    struct CS_JsonNode *alg = CS_jsonNodeByPath( keyNode, "alg" );
                    CS_FAIL_ON_NULL( alg, "Algorithm should exist.", "Nope." );
                    CS_FAIL_ON_FALSE( strlen( n->stringValue ) == n->nItemsOrLength, "Check lengths of n.", "Different." );
                    CS_FAIL_ON_FALSE( strlen( e->stringValue ) == e->nItemsOrLength, "Check lengths of e.", "Different." );
                    int nSize;
                    void *nBits = CS_base64DecodeUrlTemp( n->stringValue, n->nItemsOrLength, &nSize );
                    int eSize;
                    void *eBits = CS_base64DecodeUrlTemp( e->stringValue, e->nItemsOrLength, &eSize );
                    CS_FAIL_ON_NULL( nBits, "Decode N.", "Nope." );
                    CS_FAIL_ON_NULL( eBits, "Decode E.", "Nope." );
                    
                    BIGNUM *bn = BN_bin2bn( nBits, nSize, NULL );
                    CS_FAIL_ON_NULL( bn, "Make BIGNUM out of nbits", "Nope." );
                    BIGNUM *be = BN_bin2bn( eBits, eSize, NULL );
                    CS_FAIL_ON_NULL( bn, "Make BIGNUM out of ebits", "Nope." );

                    //Okay, this isn't well documented. So, we grab a param builder.
                    //Stuff in our 'n' and 'e' parameters
                    //https://docs.openssl.org/3.3/man7/EVP_PKEY-RSA/
                    OSSL_PARAM_BLD *param_builder = OSSL_PARAM_BLD_new();
                    CS_FAIL_ON_NULL( param_builder, "Parameter builder", "Nope!" );
                    if( param_builder ) {
                        CS_FAIL_ON_FALSE( OSSL_PARAM_BLD_push_BN( param_builder, "n", bn ) == 1, "Push n", "Failed." );
                        CS_FAIL_ON_FALSE( OSSL_PARAM_BLD_push_BN( param_builder, "e", be ) == 1, "Push e", "Failed." );
                        OSSL_PARAM *param = OSSL_PARAM_BLD_to_param( param_builder );
                        CS_FAIL_ON_NULL( param, "Convert parameter builder to parameters.", "Failed." );

                        if( param ) {
                            //Generate a public key context...and then tell it we're going
                            //to use the fromdata thing on it.
                            EVP_PKEY_CTX *pkey_context = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
                            CS_FAIL_ON_NULL( pkey_context, "Public key context creation.", "Failed." );
                            if( pkey_context ) {
                                CS_FAIL_ON_FALSE( EVP_PKEY_fromdata_init( pkey_context ) == 1, "Fromdata init.", "Failed." );
                                //Call the fromdata thing and have it output the key.
                                EVP_PKEY *pkey = NULL;
                                CS_FAIL_ON_FALSE( EVP_PKEY_fromdata( pkey_context, &pkey, EVP_PKEY_PUBLIC_KEY, param ) == 1, "Create PKEY from the parameters.", "Failed." );
                                CS_FAIL_ON_NULL( pkey, "Finally, pkey generated.", "Nope." );
                                if( pkey ) {
                                    //Now, grab a message digest context thing.
                                    EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
                                    CS_FAIL_ON_NULL( mdctx, "Message Digest CTX.", "Failed." );
                                    if( mdctx ) {
                                        struct CS_StringBuilder *sb = CS_SB_create( 1024 );
                                        EVP_PKEY_CTX *newCtx;
                                        CS_SB_append( sb, jwt->header );
                                        CS_SB_appendChar(sb,'.');
                                        CS_SB_append(sb, jwt->payload );
                                        CS_FAIL_ON_FALSE( EVP_DigestVerifyInit(mdctx, &newCtx, EVP_sha256(), NULL, pkey ) == 1, "Init digest.", "Failed" );
                                        CS_FAIL_ON_FALSE( EVP_DigestVerify( mdctx, jwt->signatureInBinary, jwt->binarySignatureLength, (unsigned char *)CS_SB_buffer(sb), CS_SB_size(sb) ) == 1, "Verify digest", "Failed" );
                                        CS_SB_free( sb );
                                        EVP_MD_CTX_free( mdctx );
                                    }
                                    EVP_PKEY_free( pkey );
                                }
                                EVP_PKEY_CTX_free( pkey_context );
                            }
                            OSSL_PARAM_free( param );
                        }
                        OSSL_PARAM_BLD_free( param_builder );
                        BN_free( be );
                        BN_free( bn );
                        
                    }
                }
            }
            CS_jsonFree( keysJson );
        }
        CS_jwtFree( jwt );
    }


    return testCount !=
           testSucceeded;
}

