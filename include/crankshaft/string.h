#ifndef __crankshaftstringdoth__
#define __crankshaftstringdoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

char *CS_stringCopy( const char *in );
void CS_stringFree( const char *toFree );

#ifdef __cplusplus
}
#endif
#endif
