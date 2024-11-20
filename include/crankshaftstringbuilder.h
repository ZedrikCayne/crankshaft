#ifndef __crankshaftstringbuilderdoth__
#define __crankshaftstringbuilderdoth__
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_StringBuilder {
    int originalSize;
    int currentSize;
    int currentHead;
    char *buffer;
};

#define CS_SB_size(_SB) (_SB->currentHead)
#define CS_SB_remain(_SB) (_SB->currentSize-_SB->currentHead)
#define CS_SB_printf(_SB,...) CS_SB_snprintf(_SB,0,__VA_ARGS__)

struct CS_StringBuilder *CS_SB_create( int initialSize );
struct CS_StringBuilder *CS_SB_append( struct CS_StringBuilder *buffer, const char *string );
struct CS_StringBuilder *CS_SB_appendChar( struct CS_StringBuilder *buffer, const char ch );
struct CS_StringBuilder *CS_SB_vsnprintf( struct CS_StringBuilder *buffer, int maxAppend, const char *fmt, va_list ap );
struct CS_StringBuilder *CS_SB_snprintf( struct CS_StringBuilder *buffer, int maxAppend, const char *fmt, ... );
void CS_SB_free( struct CS_StringBuilder *buffer );
const char *CS_SB_freeButReturnBuffer( struct CS_StringBuilder *buffer );
const char *CS_SB_desc( struct CS_StringBuilder *buffer );

#define CS_SB_reset(SB) (SB)->currentHead=0;(SB)->buffer[0]=0
#define CS_SB_length(SB) ((SB)->currentHead)

#ifdef __cplusplus
}
#endif
#endif
