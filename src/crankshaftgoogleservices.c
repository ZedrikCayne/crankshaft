#include <stdlib.h>
#include <stdio.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftjson.h"

#include "crankshaftstringbuilder.h"
#include "crankshaftgoogleservices.h"
#include "crankshafthttp.h"

static const char *googleKeysEndpoint = "https://www.googleapis.com/oauth2/v3/certs";

static struct CS_JsonNode *googleKeys = NULL;

bool CS_GS_initWithFile( const char * pathToGoogleServicesJson ) {
    return false;
}
bool CS_GS_kill( void ) {
    return false;
}

const char *CS_GS_getClientID() {
    return NULL;
}

#define DEFAULT_KEY_SIZE 8192

bool CS_GS_getKeys(void) {
    struct CS_RequestReply *reply = 
        CS_httpMakeRequest( CS_HTTP_METHOD_GET,
                            googleKeysEndpoint,
                            NULL, 0,
                            NULL, 0,
                            NULL, 0,
                            NULL );
    if( reply == NULL ) return true;
    googleKeys = CS_jsonParseCopy( CS_PP_startOfData(reply->buffer),
                                   CS_PP_dataSize(reply->buffer),
                                   DEFAULT_KEY_SIZE );
    if( googleKeys == NULL ) return true;
    if( CS_jsonNodeToUnquoted( googleKeys, true ) == NULL ) {
        CS_LOG_ERROR("Failed to unquote the reply");
        CS_jsonFree(googleKeys);
        return true;
    }
    CS_httpCloseRequest( reply );

    struct CS_StringBuilder *sb = CS_jsonNodePrintable( googleKeys );
    if( sb ) {
        CS_LOG_LOUD("Keys as Json: %s", sb->buffer);
    }
    

    return false;
}

struct CS_StringBuilder *CS_GS_getKeysDesc(void) {
    return googleKeys?CS_jsonNodePrintable( googleKeys ):NULL;
}



