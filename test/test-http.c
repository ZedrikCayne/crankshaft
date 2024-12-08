#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"

#include "crankshafthttp.h"
#include "crankshaftserver.h"
#include "crankshaftjson.h"

extern bool test_http(void);

static int testCount = 0;
static int testSucceeded = 0;

static struct CS_Route testRoutes[] = {
    { CS_HTTP_METHOD_CONNECT,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 },
    { CS_HTTP_METHOD_DELETE,   CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 },
    { CS_HTTP_METHOD_HEAD,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 },
    { CS_HTTP_METHOD_POST,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 },
    { CS_HTTP_METHOD_PUT,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 },
    { CS_HTTP_METHOD_TRACE,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 },
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_Diagnostic200 }
};

bool test_http(void) {
    //Tests go here:

    struct CS_WebServer *testServer = CS_StartWebServer( 0, NULL, NULL, "localhost", "root", "index.html", 0, testRoutes, sizeof(testRoutes)/sizeof(testRoutes[0]) );
    //struct CS_WebServer *testServer = CS_StartWebServer( 0, NULL, NULL, NULL, "root", "index.html", 0, testRoutes, sizeof(testRoutes)/sizeof(testRoutes[0]) );
    CS_FAIL_ON_NULL( testServer, "Start web server.", "Failed." );
    char base[ 128 ];
    snprintf( base, 128, "https://localhost:%d/test", testServer->serverPort );
    if( testServer ) {
        CS_httpInitSSL();

        struct CS_RequestReply * reply = CS_httpMakeRequest( CS_HTTP_METHOD_GET, base, NULL, 0, NULL, 0, NULL, 0, NULL );
        CS_FAIL_ON_NULL( reply, "GET request to /test", "Failed" );
        if( reply ) {
            struct CS_JsonNode *json = CS_jsonParse( CS_PP_startOfData( reply->buffer ),
                    CS_PP_dataSize( reply->buffer ), 512 );
            CS_FAIL_ON_NULL( json, "Reply should be json.", "Did not parse as json." );
            if( json ) {
                //Basic structure, so we're expecting no data or variables.
                struct CS_JsonNode *headers = CS_jsonNodeByPath( json, "headers" );
                struct CS_JsonNode *userAgent = NULL;
                if( headers ) {
                    CS_JSON_NODE_ITER(headers,aHeader) {
                        userAgent = CS_jsonNodeByPath( aHeader, "name");
                        if( userAgent && strcmp(userAgent->stringValue, "User-Agent") == 0 ) break;
                        userAgent = NULL;
                        aHeader = aHeader->next;
                    }
                    CS_FAIL_ON_NULL( userAgent, "Looking for User-Agent.", "Not found!" );
                    if( userAgent ) {
                        struct CS_JsonNode *userAgentValue = CS_jsonNodeByPath( userAgent->up, "value" );
                        CS_FAIL_ON_NULL( userAgentValue, "Look up value of User-Agent.", "Couldn't find 'value'");
                        CS_FAIL_ON_FALSE( strcmp(userAgentValue->stringValue, "Crankshaft") == 0, "User-Agent should be Crankshaft", "Was %s", userAgentValue->stringValue?userAgentValue->stringValue:"NULL");
                    }
                }
                CS_jsonFree( json );
            }
            CS_httpCloseRequest( reply );
        }

        snprintf(base, 128, "https://localhost:%d/test?a=b&c=fah&d=groovy",testServer->serverPort);

        reply = CS_httpMakeRequest( CS_HTTP_METHOD_POST, base, NULL, 0, NULL, 0, NULL, 0, NULL );
        CS_FAIL_ON_NULL( reply, "Make POST with uri parameters.", "Failed on %s", base );
        if( reply ) {
            struct CS_JsonNode *json = CS_jsonParseCopy( CS_PP_startOfData( reply->buffer ),
                    CS_PP_dataSize( reply->buffer ), 512 );
            if( json ) {
                struct CS_JsonNode *queryParams = CS_jsonNodeByPath( json, "queryParameters" );
                CS_FAIL_ON_NULL( queryParams, "Check Query Parameters", "Could not find query parameters. %s", CS_PP_startOfData( reply->buffer ) );
                CS_JSON_NODE_ITER(queryParams, queryParameter) {
                    struct CS_JsonNode *name = CS_jsonNodeByPath( queryParameter, "name" );
                    struct CS_JsonNode *value = CS_jsonNodeByPath( queryParameter, "value" );
                    if( name && value ) {
                        switch( name->stringValue[0] ) {
                            case 'a':
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "b" ) == 0, "a is b", "Not." );
                                break;
                            case 'c':
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "fah" ) == 0, "a is b", "Not." );
                                break;
                            case 'd':
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "groovy" ) == 0, "a is b", "Not." );
                                break;
                            default: 
                                CS_FAIL_ON_TRUE( true, "Any other 'name'", "Name was: %s", name->stringValue );
                                break;
                        }
                    }
                }
                CS_jsonFree( json );
            }
            CS_httpCloseRequest(reply);
        }

        reply = CS_httpMakeRequest( CS_HTTP_METHOD_POST, base, NULL, 0, NULL, 0, NULL, 0, NULL );
        if( reply ) {
            CS_httpCloseRequest(reply);
        }

        CS_LOG_TRACE("Kill ssl.");
        CS_httpKillSSL();
        CS_httpCleanupReplies();
        CS_KillWebServer( testServer );
    }


    return testCount !=
           testSucceeded;
}


