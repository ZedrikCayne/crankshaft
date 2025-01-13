#ifndef __crankshaftgoogleservicesdoth__
#define __crankshaftgoogleservicesdoth__
#include <stdbool.h>

#include <crankshaft/stringbuilder.h>
#include <crankshaft/jwt.h>

#ifdef __cplusplus
extern "C" {
#endif

bool CS_GS_initWithFile( const char * pathToGoogleServicesJson );
bool CS_GS_initWithEnvironmentVariable( const char *variableName );
bool CS_GS_initWithEmbeddedJson( const char *json );
bool CS_GS_kill( void );

const char *CS_GS_getClientID( void );

bool CS_GS_getKeys( void );
struct CS_StringBuilder *CS_GS_getKeysDesc(void);

bool CS_GS_verifyJwt( const struct CS_Jwt *jwt );

#ifdef __cplusplus
}
#endif
#endif
