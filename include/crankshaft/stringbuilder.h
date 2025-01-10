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
#define CS_SB_writePosition(_SB) (_SB->buffer + _SB->currentHead)
#define CS_SB_fakeAppend(_SB,amount) {_SB->currentHead+=amount;_SB->buffer[_SB->currentHead]=0;}
#define CS_SB_getPushPullBuffer(_SB) CS_PP_onStaticBuffer((_SB)->currentHead,(_SB)->buffer)
#define CS_SB_buffer(_SB) (_SB->buffer)

struct CS_StringBuilder *CS_SB_create( int initialSize );
struct CS_StringBuilder *CS_SB_append( struct CS_StringBuilder *buffer, const char *string );
struct CS_StringBuilder *CS_SB_appendChar( struct CS_StringBuilder *buffer, const char ch );
struct CS_StringBuilder *CS_SB_vsnprintf( struct CS_StringBuilder *buffer, int maxAppend, const char *fmt, va_list ap );
struct CS_StringBuilder *CS_SB_snprintf( struct CS_StringBuilder *buffer, int maxAppend, const char *fmt, ... );
bool CS_SB_expandBy( struct CS_StringBuilder *buffer, int minimumNewCapacity );
void CS_SB_free( struct CS_StringBuilder *buffer );
const char *CS_SB_freeButReturnBuffer( struct CS_StringBuilder *buffer );
const char *CS_SB_desc( struct CS_StringBuilder *buffer );

#define CS_SB_reset(SB) (SB)->currentHead=0;(SB)->buffer[0]=0
#define CS_SB_length(SB) ((SB)->currentHead)

#ifdef __cplusplus
}
#endif
#endif
