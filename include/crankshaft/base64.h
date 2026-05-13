#ifndef __crankshaftbase64doth__
#define __crankshaftbase64doth__
#include <stdbool.h>

#include <crankshaft/stringbuilder.h>
#include <stdint.h>

/********************************************************************
 *
 * Base 64 encoding/decoding utilities.
 *
 * Functions ending in 'Temp' will use the global temp buffers as
 * the output, no need to free the results. The ones postpended
 * with LinearAlloc tack the output results onto a linear allocator.
 * Freeing these pointers will just blow up.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

char *CS_base64Encode( void *toEncode, int32_t length, int32_t *outputLength );
char *CS_base64EncodeTemp( void *toEncode, int32_t length, int32_t *outputLength );
struct CS_StringBuilder *CS_base64EncodeAppend( const void *toEncode, int32_t length,  struct CS_StringBuilder *appendTo );

void *CS_base64Decode( const char *toDecode, int32_t length, int32_t *outputLength );
void *CS_base64DecodeTemp( const char *toDecode, int32_t length, int32_t *outputLength );
void *CS_base64DecodeInPlace( char *toDecode, int32_t length, int32_t *outputLength );
void *CS_base64DecodeLinearAlloc( const char *toDecode, int32_t length, int32_t *outputLength, void *linearAllocator );
void *CS_base64DecodeUrl( const char *toDecode, int32_t length, int32_t *outputLength );
void *CS_base64DecodeUrlTemp( const char *toDecode, int32_t length, int32_t *outputLength );
void *CS_base64DecodeUrlInPlace( char *toDecode, int32_t length, int32_t *outputLength );
void *CS_base64DecodeUrlLinearAlloc( const char *toDecode, int32_t length, int32_t *outputLength, void *linearAllocator );

#ifdef __cplusplus
}
#endif
#endif
