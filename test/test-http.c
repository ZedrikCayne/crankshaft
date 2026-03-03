#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/http.h>
#include <crankshaft/server.h>
#include <crankshaft/json.h>

extern bool test_http(void);

static int testCount = 0;
static int testSucceeded = 0;

static char *testData = "Le Test Data";

static void dcCallback( struct CS_ClientInfo *info ) {
    info->persistentData = NULL;
}

static bool grundle( struct CS_ClientInfo *info ) {
    info->disconnectCallback = dcCallback;
    info->persistentData = testData;
    return false;
}
static bool grundle2( struct CS_ClientInfo *info ) {
    if( info->persistentData != testData )
        return true;
    return false;
}

static struct CS_Route testRoutes[] = {
    { CS_HTTP_METHOD_ANY, CS_ROUTE_TYPE_FILTER, 0, "czech", grundle },
    { CS_HTTP_METHOD_ANY, CS_ROUTE_TYPE_FILTER, 0, "czech2", grundle2 },
    { CS_HTTP_METHOD_CONNECT,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_DELETE,   CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_HEAD,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_POST,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_PUT,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_TRACE,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 }
};

static struct CS_Route failRoute[] = {
    { CS_HTTP_METHOD_ANY, CS_ROUTE_TYPE_FILTER, 0, "czech", grundle2 },
    { CS_HTTP_METHOD_CONNECT,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_DELETE,   CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_HEAD,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_POST,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_PUT,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_TRACE,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 },
    { CS_HTTP_METHOD_GET,  CS_ROUTE_TYPE_PREFIX, 0, "/test", CS_serverDiagnostic200 }
};

bool test_http(void) {
    //Tests go here:
    struct CS_WebServer *testServer = CS_serverStart( 0, NULL, NULL, NULL, "root", "index.html", 0, failRoute, sizeof(failRoute)/sizeof(failRoute[0]) );
    CS_FAIL_ON_NULL( testServer, "Start fail web server.", "Failed." );
    char base[ 128 ];
    snprintf( base, 128, "http://localhost:%d/test", testServer->serverPort );

    if( testServer ) {
        struct CS_RequestReply * reply = CS_httpMakeRequest( CS_HTTP_METHOD_GET, base, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL );
        CS_FAIL_ON_NOT_NULL( reply, "Request should come back null.", "Oops." );
        CS_serverKill( testServer );
    }

    testServer = CS_serverStart( 0, NULL, NULL, "localhost", "root", "index.html", 0, testRoutes, sizeof(testRoutes)/sizeof(testRoutes[0]) );
    CS_FAIL_ON_NULL( testServer, "Start web server.", "Failed." );
    snprintf( base, 128, "https://localhost:%d/test", testServer->serverPort );
    if( testServer ) {
        struct CS_RequestReply * reply = CS_httpMakeRequest( CS_HTTP_METHOD_GET, base, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL );
        CS_FAIL_ON_NULL( reply, "GET request to /test", "Failed" );
        if( reply ) {
            struct CS_JsonNode *json = CS_jsonParse( CS_PP_startOfData( reply->buffer ),
                    CS_PP_dataSize( reply->buffer ), 512 );
            CS_FAIL_ON_NULL( json, "Reply should be json.", "Did not parse as json." );
            if( json ) {
                //Basic structure, so we're expecting no data or variables.
                struct CS_JsonNode *userAgentValue = CS_jsonNodeByPath( json, "headers/|name=User-Agent/value" );
                CS_FAIL_ON_NULL( userAgentValue, "Look for user agent.", "Could not find user agent." );
                if( userAgentValue )
                    CS_FAIL_ON_FALSE( userAgentValue && strcmp(userAgentValue->stringValue, "Crankshaft") == 0, "User-Agent should be Crankshaft", "Was %s", userAgentValue->stringValue?userAgentValue->stringValue:"NULL");
                CS_jsonFree( json );
            }
            CS_httpCloseRequest( reply );
        }

        snprintf(base, 128, "https://localhost:%d/test?a=b&c=fah&d=groovy",testServer->serverPort);

        reply = CS_httpMakeRequest( CS_HTTP_METHOD_POST, base, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL );
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
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "fah" ) == 0, "c is fah", "Not." );
                                break;
                            case 'd':
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "groovy" ) == 0, "d is groovy", "Not." );
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

        struct CS_QueryParameter queryParameters[] = {
            {"a", "b"},
            {"c", "fah"},
            {"d", "groovy"}
        };

        snprintf(base, 128, "https://localhost:%d/test",testServer->serverPort);
        struct CS_RequestHeader headers[] = {
            {"Header1","Value1"},
            {"Header2","Value2"},
            {"Header3","Value3"},
            {"Header4","Value4"},
        };
        reply = CS_httpMakeRequest( CS_HTTP_METHOD_POST, base, headers, 4, queryParameters, 3, NULL, 0, NULL, 0, NULL );
        CS_FAIL_ON_NULL( reply, "Make POST with uri parameters and form parameters.", "Failed on %s", base );
        if( reply ) {
            const char *ctype = CS_httpReplyHeader(reply,"Content-Type");
            CS_FAIL_ON_NULL( ctype, "Content type header.", "Got a null." );
            CS_FAIL_ON_FALSE( ctype && strcmp(ctype,"application/json") == 0, "Should have gotten json back.", "Got %s instead", ctype?ctype:"NULL" );
            struct CS_JsonNode *json = CS_jsonParseCopy( CS_PP_startOfData( reply->buffer ),
                    CS_PP_dataSize( reply->buffer ), 512 );
            CS_FAIL_ON_NULL( json, "Json parsing reply", "Json failed." );
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
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "fah" ) == 0, "c is fah", "Not." );
                                break;
                            case 'd':
                                CS_FAIL_ON_FALSE( strcmp( value->stringValue, "groovy" ) == 0, "d is groovy", "Not." );
                                break;
                            default: 
                                CS_FAIL_ON_TRUE( true, "Any other 'name'", "Name was: %s", name->stringValue );
                                break;
                        }
                    }
                }for( int i = 0; i < sizeof(headers)/sizeof(headers[0]); ++i ) {
                    const char *tempPath = CS_tempBuffSnprintf( 64, "headers/|name=%s/value", headers[i].header );
                    const char *tempVal = CS_jsonNodeValueAsTempString(CS_jsonNodeByPath( json, tempPath ) );
                    CS_FAIL_ON_FALSE( tempVal && strcmp(tempVal,headers[i].values) == 0, CS_tempBuffSnprintf(64, "Looking for %s in %s", headers[i].values, tempPath), "Found %s", tempVal?tempVal:NULL );
                }
                CS_jsonFree( json );
            }
            CS_httpCloseRequest(reply);
        }

        struct CS_FormParameters formParameters[] = {
            {"form1","Form1 Data"},
            {"form2","Form2 Data"},
            {"form3","Form3 Data"}
        };

        reply = CS_httpMakeRequest( CS_HTTP_METHOD_POST, base, headers, 4, NULL, 0, formParameters, 3, NULL, 0, NULL );
        CS_FAIL_ON_NULL( reply, "Request with params, form params, uri paramaters.", "Failed." );
        if( reply ) {
            struct CS_JsonNode *json = CS_jsonParseCopy( CS_PP_startOfData( reply->buffer ),
                    CS_PP_dataSize( reply->buffer ), 512 );
            if( json ) {
                for( int i = 0; i < sizeof(formParameters)/sizeof(formParameters[0]); ++i ) {
                    const char *tempPath = CS_tempBuffSnprintf( 64, "formParameters/|name=%s/value", formParameters[i].name);
                    const char *tempVal = CS_jsonNodeValueAsTempString(CS_jsonNodeByPath( json, tempPath ) );
                    CS_FAIL_ON_FALSE( tempVal && strcmp(tempVal,formParameters[i].value)==0, tempPath, "No match." );

                }
                CS_jsonFree( json );
            }
            CS_httpCloseRequest( reply );
        }
        CS_serverKill( testServer );
    }
    CS_httpCleanupReplies();


    return testCount !=
           testSucceeded;
}


