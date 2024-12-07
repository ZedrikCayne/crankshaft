#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttempbuff.h"
#include "crankshaftbase64.h"

#include "crankshaftjwt.h"

//struct CS_Jwt {
//    const char *header;
//    const char *payload;
//    const char *signature;
//};

enum {
    PARSE_HEADER,
    PARSE_PAYLOAD,
    PARSE_SIGNATURE,
    PARSE_END
};

static struct CS_Jwt *privateJwtParse( char *inJwt,
       int jwtLength,
       struct CS_Jwt *outJwt ) {
    char *in = inJwt;
    char *inEnd = inJwt + jwtLength;
    char *startOfCurrentWebTokenBit = NULL;
    int parseState = PARSE_HEADER;
    outJwt->header = NULL;
    outJwt->payload = NULL;
    outJwt->signature = NULL;
    while( in < inEnd ) {
        if( startOfCurrentWebTokenBit == NULL )
            startOfCurrentWebTokenBit = in;
        if( *in == '.' || *in == 0 || in == (inEnd-1) ) {
            int nLen = in - startOfCurrentWebTokenBit;
            if( in == inEnd-1) nLen += 1;
            int outLen = 0;
            void *out = CS_base64DecodeInPlace( startOfCurrentWebTokenBit, nLen, &outLen );
            if( !out ) return NULL;
            switch( parseState ) {
                case PARSE_HEADER:
                    outJwt->header = startOfCurrentWebTokenBit;
                    break;
                case PARSE_PAYLOAD:
                    outJwt->payload = startOfCurrentWebTokenBit;
                    break;
                case PARSE_SIGNATURE:
                    outJwt->signature = startOfCurrentWebTokenBit;
                    break;
            }
            ++parseState;
            *(startOfCurrentWebTokenBit + nLen) = 0;
            startOfCurrentWebTokenBit = NULL;
            if( parseState == PARSE_END )
                break;
        }
        ++in;
    }
    if( parseState < PARSE_PAYLOAD ) {
        CS_LOG_ERROR("JWT: Needs at least a header and payload.");
    }
    return outJwt;
}

const struct CS_Jwt *CS_jwtParse( const char *jwt, int jwtLength ) {
    struct CS_Jwt *tBuff = (struct CS_Jwt *)CS_alloc( jwtLength + sizeof(struct CS_Jwt) );
    if( tBuff == NULL ) return NULL;
    char *jwtCopy = (char *)(tBuff + 1);
    strncpy( jwtCopy, jwt, jwtLength );
    struct CS_Jwt *returnValue = privateJwtParse( jwtCopy, jwtLength, tBuff );
    if( returnValue == NULL ) CS_free(tBuff);
    return returnValue;
}

const struct CS_Jwt *CS_jwtParseInPlace( char *jwt, int jwtLength ) {
    struct CS_Jwt *tBuff = (struct CS_Jwt *)CS_alloc( sizeof( struct CS_Jwt ) );
    if( tBuff == NULL ) return NULL;
    struct CS_Jwt *returnValue = privateJwtParse( jwt, jwtLength, tBuff );
    if( returnValue == NULL ) CS_free( tBuff );
    return returnValue;
}

const struct CS_Jwt *CS_jwtParseTemp( const char *jwt, int jwtLength ) {
    struct CS_Jwt *tBuff = (struct CS_Jwt *)CS_tempBuff( jwtLength + sizeof(struct CS_Jwt) );
    char *jwtCopy = (char *)(tBuff + 1);
    strncpy( jwtCopy, jwt, jwtLength );
    return privateJwtParse( jwtCopy, jwtLength, tBuff );
}

void CS_jwtFree( struct CS_Jwt *jwt ) {
    CS_free( jwt );
}

