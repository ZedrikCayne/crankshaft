#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <openssl/sha.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/base64.h>

extern bool test_websocket(void);

static int testCount = 0;
static int testSucceeded = 0;

static const char *wsAcceptConcat = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char *testThing   = "dGhlIHNhbXBsZSBub25jZQ==";
static const char *outputThing = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

bool test_websocket(void) {
    //Tests go here:
    unsigned char outgoingHash[ SHA_DIGEST_LENGTH ];
    const char *combined = CS_tempBuffSnprintf( 128, "%s%s", testThing, wsAcceptConcat );
    CS_LOG_VERBOSE( "Combined: %s", combined );
    int length = strlen( combined );
    SHA1( (const unsigned char*)combined, length, outgoingHash );
    const char *encoded = CS_base64EncodeTemp( outgoingHash, SHA_DIGEST_LENGTH, NULL );
    CS_LOG_VERBOSE( "Encoded: %s", encoded );
    CS_FAIL_ON_FALSE( strncmp( encoded, outputThing, strlen(outputThing) ) == 0, "Match output.", "Wanted \n%s\n%s", outputThing, encoded );


    return testCount !=
           testSucceeded;
}


