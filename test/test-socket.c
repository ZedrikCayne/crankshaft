#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/server.h>
#include <crankshaft/http.h>
#include <crankshaft/socket.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/json.h>

extern bool test_socket(void);

static int testCount = 0;
static int testSucceeded = 0;

static struct CS_Route testRoutes[] = {
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 }
};

bool test_socket(void) {
    //Tests go here:

    struct CS_WebServer *testServer = CS_serverStart( 0, NULL, NULL, "localhost", "root", "index.html", 0, testRoutes, sizeof(testRoutes)/sizeof(testRoutes[0]) );
    CS_FAIL_ON_NULL( testServer, "Start web server.", "Failed." );
    if( testServer ) {
        struct CS_Socket *testSocket = CS_socketConnect( "localhost", testServer->serverPort, true, false, 2048, 2048, false, false );
        CS_FAIL_ON_NULL( testSocket, "Connect to webserver.", "Failed" );
        if( testSocket ) {
            struct CS_PushPullBuffer *ppOutput = CS_socketLockOutputBuffer( testSocket );
            CS_FAIL_ON_NULL( ppOutput, "Lock output buffer", "NULL" );
            if( ppOutput ) {
                CS_PP_printf( ppOutput, "GET /test HTTP/1.1\r\n\r\n" );
                CS_socketUnlockOutputBuffer( testSocket );
                int length = CS_socketEmptyOutputBuffer( testSocket, true );
                CS_FAIL_ON_TRUE( length < 0, "Send the request.", "Failed" );
                length = CS_socketFillIncomingBuffer( testSocket, true );
                CS_FAIL_ON_TRUE( length < 0, "Send the request.", "Failed" );
                struct CS_PushPullBuffer *ppInput = CS_socketLockInputBuffer( testSocket );
                CS_FAIL_ON_NULL( ppInput, "Lock input buffer", "NULL" );
                char *start = CS_PP_startOfData( ppInput );
                for( int i = 0; i < CS_PP_dataSize( ppInput ); ++i ) {
                    if( *start == '{' ) break;
                    ++start;
                }
                CS_FAIL_ON_FALSE( start < CS_PP_endOfData( ppInput ), "Did we find a {?", "Nope." );
                if( start < CS_PP_endOfData( ppInput ) ) {
                    struct CS_JsonNode *json = CS_jsonParse( start, CS_PP_endOfData(ppInput)-start, 1024 );
                    CS_FAIL_ON_NULL( json, "Should be a bunch of json from the CS_Diagnostic200.", "Nope." );
                    if( json ) CS_jsonFree( json );
                }
            }
            CS_socketDestroy( testSocket );
        }
        CS_serverKill( testServer );
    }

    return testCount !=
           testSucceeded;
}


