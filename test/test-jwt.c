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

static const char *testJwt = "eyJhbGciOiJSUzI1NiIsImtpZCI6IjJjOGEyMGFmN2ZjOThmOTdmNDRiMTQyYjRkNWQwODg0ZWIwOTM3YzQiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL2FjY291bnRzLmdvb2dsZS5jb20iLCJhenAiOiI5NjMyMTU5NzI5MjEtOXZsYmV0bjA0cGVicTRjZHNudWI1cHZlczIwcHFwOXEuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJhdWQiOiI5NjMyMTU5NzI5MjEtOXZsYmV0bjA0cGVicTRjZHNudWI1cHZlczIwcHFwOXEuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJzdWIiOiIxMTYwMjQ2MTQ3MDk4MDA2MTQxOTAiLCJlbWFpbCI6InplZHJpa2NheW5lQGdtYWlsLmNvbSIsImVtYWlsX3ZlcmlmaWVkIjp0cnVlLCJuYmYiOjE3MzM1Mzc1MzUsIm5hbWUiOiJaZWRyaWsgQ2F5bmUiLCJwaWN0dXJlIjoiaHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnRlbnQuY29tL2EvQUNnOG9jS3I3cWRmM0RkdGNha3NIQW96bV9LeWJhQjBnNnRiNUFzQl9uU1d2d1lBemNzZ0MyWGM1dz1zOTYtYyIsImdpdmVuX25hbWUiOiJaZWRyaWsiLCJmYW1pbHlfbmFtZSI6IkNheW5lIiwiaWF0IjoxNzMzNTM3ODM1LCJleHAiOjE3MzM1NDE0MzUsImp0aSI6IjZmMTg1NTZkZjNiNmM2OTY5ZDUzNGZjYmY5Nzg5MzQ4YTZiMTZkYmEifQ.j9FSg_d7lKHpYlYEDV55Vf7AvIPhsKSC2I6ruDQUdQTm6_iVel-O5o0WZ3qEFOOMHthP-LJmNEkZib3YCm7lUfBOEkwcVAxcc6inT7LYqi8qy0Kf9vEiuiUmB7vI6kw2QCer88Ub7EosFlRGYeF4gqh31lGOB0l9D_vK90Pccf";

static const char *testHeader = "{\"alg\":\"RS256\",\"kid\":\"2c8a20af7fc98f97f44b142b4d5d0884eb0937c4\",\"typ\":\"JWT\"}";
static const char *testPayload= "{\"iss\":\"https://accounts.google.com\",\"azp\":\"963215972921-9vlbetn04pebq4cdsnub5pves20pqp9q.apps.googleusercontent.com\",\"aud\":\"963215972921-9vlbetn04pebq4cdsnub5pves20pqp9q.apps.googleusercontent.com\",\"sub\":\"116024614709800614190\",\"email\":\"zedrikcayne@gmail.com\",\"email_verified\":true,\"nbf\":1733537535,\"name\":\"Zedrik Cayne\",\"picture\":\"https://lh3.googleusercontent.com/a/ACg8ocKr7qdf3DdtcaksHAozm_KybaB0g6tb5AsB_nSWvwYAzcsgC2Xc5w=s96-c\",\"given_name\":\"Zedrik\",\"family_name\":\"Cayne\",\"iat\":1733537835,\"exp\":1733541435,\"jti\":\"6f18556df3b6c6969d534fcbf9789348a6b16dba\"}";

static const char *testKeys = "{\"keys\":[{\"kid\":\"2c8a20af7fc98f97f44b142b4d5d0884eb0937c4\",\"n\":\"yV3njE5D9TYYhIxXe32dXkGQyqteajUJ8nUqIR-mrp7sdWQsE9__Nxkvg4PbuCpYzRyiK0iK8i02_G6lqDB_1_XUGBCA3OdJ8sGOaSynzLW3B_qdP8jD9HCTkaXmSFKIjiToFFg8QVMEimdl428HwH4UCLROtHWhwt-Y7X3JvwAo7CL1DxtE4FI83Y2BhJ4LFr8M-nijZxaM9WREaHT6D97EKELixHM66opoM_gwLVK_BtRhM1KUrE82EP8mmajPRVyJcUeNsEyDDnfp2ehkHBi0npkslT7I32kqavOYvhac3feGrhtdoCj6fnTjhXAZ_f5NQ2lIfA1ldZEpZDEg2w\",\"use\":\"sig\",\"kty\":\"RSA\",\"e\":\"AQAB\",\"alg\":\"RS256\"},{\"alg\":\"RS256\",\"n\":\"p4FEhRwWtJxTdllpjXeSQzAdDqQQhmByHHUkjjERSOAaqyqz8eIWOOulAar3oGpa4HyIJoA5bx7J3pmerfQsX1MaTF1vyWAVnDsgoUS6jfGnrxerhWrC8GotcBewM4wuI-1uT1Edng8Req6bP1WxW1fUATd1TabNLDW-wy4OVvL45b82g4wHGMWxreILdC8xdlyxcsPvh5gr5CHgxPAh-f3X6l_IJePJs6m_auYRmW8Cban84KaLS0NzZuydxvRPA4jozIGl1E08-szCJnjjvn-nVkLYHuJf8DYCp9HWT0_2kSsumqs0PFFnEdngFsPzpD7EuGGs0BTARJFMtmCrWQ\",\"kty\":\"RSA\",\"kid\":\"564feacec3ebdfaa7311b9d8e73c42818f291264\",\"e\":\"AQAB\",\"use\":\"sig\"}]}";

bool test_jwt(void) {
    //Tests go here:
    const struct CS_Jwt *jwt = CS_jwtParse( testJwt, strlen(testJwt), 8192 );

    struct CS_JsonNode *keysJson = CS_jsonParseCopy( testKeys, strlen(testKeys), 1024 );

    CS_FAIL_ON_NULL( jwt, "Create JWT.", "Failed." );

    if( jwt ) {
        struct CS_JsonNode *js = CS_jsonParseCopy(testHeader, strlen(testHeader), 1024);
        struct CS_JsonNode *js2 = CS_jsonParseCopy(testPayload, strlen(testPayload), 1024);
        CS_jsonFree( js );
        CS_jsonFree( js2 );
        if( keysJson ) {
            struct CS_JsonNode *keyIdNode = CS_jsonNodeByPath( jwt->jsonHeader, "kid" );
            if( keyIdNode ) {
                struct CS_JsonNode *keyNode = CS_jsonNodeByPath( keysJson, CS_tempBuffSnprintf( 128, "keys/|kid=%s", keyIdNode->stringValue ) );
                CS_FAIL_ON_NULL( keyNode, "Grab the key out of the keys.", "Failed." );
                struct CS_JsonNode *n = CS_jsonNodeByPath( keyNode, "n" );
                struct CS_JsonNode *e = CS_jsonNodeByPath( keyNode, "e" );
                struct CS_JsonNode *alg = CS_jsonNodeByPath( keyNode, "alg" );
                CS_FAIL_ON_NULL( alg, "Algorithm should exist.", "Nope." );
                CS_FAIL_ON_FALSE( strlen( n->stringValue ) == n->nItemsOrLength, "Check lengths of n.", "Different." );
                CS_FAIL_ON_FALSE( strlen( e->stringValue ) == e->nItemsOrLength, "Check lengths of e.", "Different." );
                int nSize;
                void *nBits = CS_base64DecodeUrl( n->stringValue, n->nItemsOrLength, &nSize );
                int eSize;
                void *eBits = CS_base64DecodeUrl( e->stringValue, e->nItemsOrLength, &eSize );
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
                                    CS_LOG( "CHECKING: %s.%s", jwt->header, jwt->payload );
                                    CS_SB_append( sb, jwt->header );
                                    CS_SB_appendChar(sb,'.');
                                    CS_SB_append(sb, jwt->payload );
                                    CS_LOG( "VS      : %s", CS_SB_buffer(sb) );
                                    CS_FAIL_ON_FALSE( EVP_DigestVerifyInit(mdctx, &newCtx, EVP_sha256(), NULL, pkey ) == 1, "Init digest.", "Failed" );
                                    CS_LOG_LOUD( "Length: %d", jwt->binarySignatureLength );
                                    CS_FAIL_ON_FALSE( EVP_DigestVerify( mdctx, jwt->signatureInBinary, jwt->binarySignatureLength, (unsigned char *)CS_SB_buffer(sb), CS_SB_size(sb) ) == 1, "Verify digest", "Failed" );
                                }
                            }
                        }
                    }
                }
            }
        }
        CS_jwtFree( jwt );
    }


    return testCount !=
           testSucceeded;
}

