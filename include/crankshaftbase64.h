#ifndef __crankshaftbase64doth__
#define __crankshaftbase64doth__
#include <stdbool.h>

#include "crankshaftstringbuilder.h"

#ifdef __cplusplus
extern "C" {
#endif

char *CS_base64Encode( void *toEncode, int length, int *outputLength );
char *CS_base64EncodeTemp( void *toEncode, int length, int *outputLength );
struct CS_StringBuilder *CS_base64EncodeAppend( const void *toEncode, int length,  struct CS_StringBuilder *appendTo );

void *CS_base64Decode( const char *toDecode, int length, int *outputLength );
void *CS_base64DecodeTemp( const char *toDecode, int length, int *outputLength );
void *CS_base64DecodeInPlace( char *toDecode, int length, int *outputLength );

#ifdef __cplusplus
}
#endif
#endif
