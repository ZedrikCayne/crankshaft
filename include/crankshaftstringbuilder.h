#ifndef __crankshaftstringbuilderdoth__
#define __crankshaftstringbuilderdoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_StringBuilder {
    int originalSize;
    int currentSize;
    int currentHead;
    char *buffer;
};

struct CS_StringBuilder *CS_SB_create( int initialSize );
struct CS_StringBuilder *CS_SB_append( struct CS_StringBuilder *buffer, const char *string );
struct CS_StringBuilder *CS_SB_appendChar( struct CS_StringBuilder *buffer, const char ch );
struct CS_StringBuilder *CS_SB_printf( struct CS_StringBuilder *buffer, const char *fmt, ... );
void CS_SB_free( struct CS_StringBuilder *buffer );
const char *CS_SB_freeButReturnBuffer( struct CS_StringBuilder *buffer );
const char *CS_SB_desc( struct CS_StringBuilder *buffer );

#define CS_SB_reset(SB) (SB)->currentHead=0;(SB)->buffer[0]=0
#define CS_SB_length(SB) ((SB)->currentHead)

#ifdef __cplusplus
}
#endif
#endif
