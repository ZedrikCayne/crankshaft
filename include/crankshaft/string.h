#ifndef __crankshaftstringdoth__
#define __crankshaftstringdoth__
#include <stdbool.h>
#include <stdint.h>

/********************************************************************
 *
 * Sized strings, compares etc to cstrings.
 * We're not going to enforce immutability. But we'll try to avoid
 * letting crankshaft do it. (We'll provid you the footgun...be
 * careful with it).
 *
 * 
 * CS_STRING_FLAG_DATA_FREE_DATA        The data pointer is allocated
 *                                      and needs to be free'd.
 * CS_STRING_FLAG_NOT_ALLOCATED         This structure was not self
 *                                      allocated.
 *
 * Generic string copy/free to enforce locality of allocation for
 * strings.
 *
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define CS_STRING_FLAG_FREE_DATA     0x00000001
#define CS_STRING_FLAG_NOT_ALLOCATED 0x00000002

struct CS_String {
    int32_t length;
    uint32_t flags;
    const char *data;
};

const struct CS_String *CS_stringCopyCstring( const char *in, int32_t length );
const struct CS_String *CS_stringReferenceCstring( const char *in, int32_t length );
const struct CS_String *CS_stringInitCopyCstring( struct CS_String *out, const char *in, int32_t length );
const struct CS_String *CS_stringInitReferenceCstring( struct CS_String *out, const char *in, int32_t length );
const struct CS_String *CS_stringCopy( struct CS_String *in );
void CS_stringFree( const struct CS_String *toFree );
int32_t CS_stringStrncmp( const struct CS_String *left, const struct CS_String *right, int32_t maxLength );
int32_t CS_stringCstrncmp( const struct CS_String *left, const char *right, int32_t length );
const char *CS_stringStrstr( const struct CS_String *haystack, const struct CS_String *needle );
const char *CS_stringCStrstr( const struct CS_String *haystack, const char *needle, int32_t length );
const char *CS_stringStrstrC( const char *haystack, int32_t length, const struct CS_String *needle );

char *CS_cstringCopy( const char *in );
void CS_cstringFree( const char *toFree );

#ifdef __cplusplus
}
#endif
#endif
