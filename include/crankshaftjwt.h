#ifndef __crankshaftjwtdoth__
#define __crankshaftjwtdoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Jwt {
    const char *header;
    const char *payload;
    const char *signature;
};

const struct CS_Jwt *CS_jwtParse( const char *jwt, int jwtLength );
const struct CS_Jwt *CS_jwtParseInPlace( char *jwt, int jwtLength );
const struct CS_Jwt *CS_jwtParseTemp( const char *jwt, int jwtLength );
void CS_jwtFree( struct CS_Jwt *jwt );

#ifdef __cplusplus
}
#endif
#endif
