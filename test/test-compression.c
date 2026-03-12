#include <crankshaft/http.h>
#include <crankshaft/server.h>
#include <crankshaft/test.h>
#include <crankshaft/logger.h>
#include <crankshaft/alloc.h>
#include <crankshaft/mime.h>
#include <string.h>
#include <stdio.h>

static bool compressed_route( struct CS_ClientInfo *info ) {
    char *bigData = (char *)CS_alloc( 1024 );
    memset( bigData, 'A', 1024 );
    struct CS_Reply *reply = CS_serverCreateReply( info, CS_RESPONSE_200, CS_MIME_TXT, bigData, 1024 );
    bool result = CS_serverDoReply( info, reply );
    CS_free( bigData );
    return result;
}

static struct CS_Route routes[] = {
    { CS_HTTP_METHOD_GET, CS_ROUTE_TYPE_EXACT, 0, "/compressed", compressed_route }
};

bool test_compression() {
    struct CS_WebServer *server = CS_serverStart( 0, NULL, NULL, NULL, NULL, NULL, 0, routes, 1 );
    if( !server ) return true;

    char url[ 128 ];
    snprintf( url, 128, "http://localhost:%d/compressed", server->serverPort );

    // Request with Accept-Encoding: gzip
    struct CS_RequestHeader headers[] = {
        { "Accept-Encoding", "gzip" }
    };

    struct CS_RequestReply *reply = CS_httpMakeRequest( CS_HTTP_METHOD_GET, url, headers, 1, NULL, 0, NULL, 0, NULL, 0, NULL );
    
    if( !reply ) {
        CS_serverKill( server );
        return true;
    }

    const char *encoding = CS_httpReplyHeader( reply, "Content-Encoding" );
    
    CS_LOG_INFO( "Content-Encoding: %s", encoding ? encoding : "none" );
    CS_LOG_INFO( "Data size: %d", CS_PP_dataSize( reply->buffer ) );

    bool failed = false;
    // If it's still gzipped, the size will likely be much smaller than 1024
    if( CS_PP_dataSize( reply->buffer ) != 1024 ) {
        CS_LOG_ERROR( "Expected 1024 bytes, got %d", CS_PP_dataSize( reply->buffer ) );
        failed = true;
    } else {
        char *data = CS_PP_startOfData( reply->buffer );
        for( int i = 0; i < 1024; i++ ) {
            if( data[i] != 'A' ) {
                CS_LOG_ERROR( "Data mismatch at %d", i );
                failed = true;
                break;
            }
        }
    }

    CS_httpCloseRequest( reply );

    // Request with Accept-Encoding: identity
    struct CS_RequestHeader headers2[] = {
        { "Accept-Encoding", "identity" }
    };

    reply = CS_httpMakeRequest( CS_HTTP_METHOD_GET, url, headers2, 1, NULL, 0, NULL, 0, NULL, 0, NULL );
    if( !reply ) {
        CS_serverKill( server );
        return true;
    }

    encoding = CS_httpReplyHeader( reply, "Content-Encoding" );
    CS_LOG_INFO( "Content-Encoding (identity request): %s", encoding ? encoding : "none" );
    
    if( encoding != NULL && strstr( encoding, "gzip" ) ) {
        CS_LOG_ERROR( "Expected no gzip encoding for identity request" );
        failed = true;
    }

    if( CS_PP_dataSize( reply->buffer ) != 1024 ) {
        CS_LOG_ERROR( "Expected 1024 bytes, got %d", CS_PP_dataSize( reply->buffer ) );
        failed = true;
    }

    CS_httpCloseRequest( reply );
    CS_serverKill( server );
    return failed;
}
