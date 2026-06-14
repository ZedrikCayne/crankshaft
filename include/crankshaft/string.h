#ifndef __crankshaftstringdoth__
#define __crankshaftstringdoth__
#include <stdbool.h>
#include <stdint.h>

#include <crankshaft/linearalloc.h>

/********************************************************************
 *
 * Sized strings.
 * 
 * We're not going to enforce immutability. But we'll try to avoid
 * letting crankshaft do it. (We'll provide you the footgun...be
 * careful with it).
 *
 * 
 * CS_STRING_FLAG_DATA_FREE_DATA        The data pointer is allocated
 *                                      and needs to be free'd.
 * CS_STRING_FLAG_ALLOCATED             This structure was not 
 *                                      allocated. Do not free it.
 *
 * It is 'technically' correct to call free on a stack or temp
 * allocated CS_String. (ie: It won't hurt) and it should be
 * done in cases where you've called CS_stringInitCopyCstring()
 *
 * Generic string copy/free to enforce locality of allocation for
 * strings. The engine built in allocation tracking complain if you
 * allocate a hunk of memory and free it in another file.
 *
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define CS_STRING_FLAG_FREE_DATA       0x00000001
#define CS_STRING_FLAG_ALLOCATED       0x00000002
#define CS_STRING_FLAG_STATIC_SIZE     0x000FFFF0
#define CS_STRING_USE_STRLEN -1
#define CS_STRING_CMP_WHOLE  -1

#define CS_STRING_STATIC_SIZE_MAX      (int32_t)0xFFFF

struct CS_String {
    int32_t length;
    uint32_t flags;
    const char *data;
};

#define CS_STRING(_WHAT) ((struct CS_String){sizeof(_WHAT)-1,0,_WHAT})
#define CS_STRING_PTR(_WHAT) ((struct CS_String){strlen(_WHAT),0,_WHAT})

#define CS_STRING_STATIC_SIZE_TYPE(_HOWBIG) \
struct CS_String##_HOWBIG {\
    int32_t length;\
    uint32_t flags;\
    const char *data;\
    char realData[ _HOWBIG ];\
};

CS_STRING_STATIC_SIZE_TYPE(8);
CS_STRING_STATIC_SIZE_TYPE(16);
CS_STRING_STATIC_SIZE_TYPE(32);
CS_STRING_STATIC_SIZE_TYPE(64);
CS_STRING_STATIC_SIZE_TYPE(128);
CS_STRING_STATIC_SIZE_TYPE(256);
CS_STRING_STATIC_SIZE_TYPE(512);
CS_STRING_STATIC_SIZE_TYPE(1024);

struct CS_String *CS_stringCopyCstring( const char *in, int32_t length );
struct CS_String *CS_stringTempCopyCstring( const char *in, int32_t length );
struct CS_String *CS_stringCopy( const struct CS_String *in );
struct CS_String *CS_stringInitCopyCstring( struct CS_String *out, const char *in, int32_t length );
struct CS_String *CS_stringLinearCopyCstring( const char *in, int32_t length, struct CS_LinearAllocator *allocator );
struct CS_String *CS_stringInitCopy( struct CS_String *out, const struct CS_String *in );
struct CS_String *CS_stringReserveTemp( int32_t length );
struct CS_String *CS_stringCopyToStatic( struct CS_String *out, const struct CS_String *in, int32_t staticSize );
struct CS_String *CS_stringCopyCstringToStatic( struct CS_String *out, int32_t staticSize, const char *in, int32_t inLength );
const struct CS_String *CS_stringReferenceCstring( const char *in, int32_t length );
const struct CS_String *CS_stringTempReferenceCstring( const char *in, int32_t length );
const struct CS_String *CS_stringInitReferenceCstring( struct CS_String *out, const char *in, int32_t length );
const struct CS_String *CS_stringInitReference( struct CS_String *out, const struct CS_String *in );
struct CS_String *CS_stringTempCopy( const struct CS_String *in );
struct CS_String *CS_stringInitCopy( struct CS_String *out, const struct CS_String *in );
void CS_stringFree( const struct CS_String *toFree );
int32_t CS_stringStrcmp( const struct CS_String *left, const struct CS_String *right );
int32_t CS_stringStrncmp( const struct CS_String *left, const struct CS_String *right, int32_t maxLength );
int32_t CS_stringStrcasecmp( const struct CS_String *left, const struct CS_String *right );
int32_t CS_stringStrncasecmp( const struct CS_String *left, const struct CS_String *right, int32_t maxLength );
const struct CS_String *CS_stringStrstr( const struct CS_String *haystack, const struct CS_String *needle );
const struct CS_String *CS_stringStrrstr( const struct CS_String *haystack, const struct CS_String *needle );
const struct CS_String *CS_stringTempStrstr( const struct CS_String *haystack, const struct CS_String *needle );
const struct CS_String *CS_stringTempStrrstr( const struct CS_String *haystack, const struct CS_String *needle );
int32_t CS_stringAtoi( const struct CS_String *toAtoi );
int64_t CS_stringAtol( const struct CS_String *toAtoi );
struct CS_String *CS_stringTempStrtok( const struct CS_String *source, const struct CS_String *delimeters, const char **savePtr );
struct CS_String *CS_stringTempStrrtok( const struct CS_String *source, const struct CS_String *delimeters, const char **savePtr );

const char *CS_stringCstring( const struct CS_String *from );
const char *CS_stringTempCstring( const struct CS_String *from );
const char *CS_stringTempCstringOrNULL( const struct CS_String *from );
const struct CS_String *CS_stringTempSnprintf(int32_t max, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
const struct CS_String *CS_stringSlice( const struct CS_String *source, int32_t startIndex, int32_t lengthOrZero );
const struct CS_String *CS_stringSliceTemp( const struct CS_String *source, int32_t startIndex, int32_t lengthOrZero );
const struct CS_String *CS_stringSliceReference( const struct CS_String *source, int32_t startIndex, int32_t lengthOrZero );
const struct CS_String *CS_stringSliceTempReference( const struct CS_String *source, int32_t startIndex, int32_t lengthOrZero );
void CS_stringLtrim( struct CS_String *trimmable );
void CS_stringRtrim( struct CS_String *trimmable );

char *CS_cstringCopy( const char *in );
void CS_cstringFree( const char *toFree );

#ifdef __cplusplus
}
#endif
#endif
