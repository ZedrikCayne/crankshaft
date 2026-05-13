#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/json.h>
#include <crankshaft/pushpull.h>
#include <crankshaft/string.h>
#include <crankshaft/storage.h>
#include <crankshaft/jwtkeychain.h>

#include <crankshaft/stringbuilder.h>
#include <crankshaft/googleservices.h>
#include <crankshaft/http.h>
#include <crankshaft/base64.h>
#include <stdint.h>

static const char *googleKeysEndpoint = "https://www.googleapis.com/oauth2/v3/certs";

static struct CS_JsonNode *googleServicesJson = NULL;
static const char *googleClientId = NULL;
static const char *googleClientSecret = NULL;

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
        int32_t nLen = strlen( varValue );
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
        int32_t nLen = strlen( json );
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


bool CS_GS_jwtVerify( const struct CS_Jwt *jwt ) {
    bool returnValue = CS_jwtVerify( jwt );
    if( !returnValue ) {
        if( !CS_jwtkeychainFetchPublicKeys( googleKeysEndpoint ) ) {
            returnValue = CS_jwtVerify( jwt );
        }
    }
    return returnValue;
}

bool CS_GS_getKeys(void) {
    return CS_jwtkeychainFetchPublicKeys( googleKeysEndpoint );
}


