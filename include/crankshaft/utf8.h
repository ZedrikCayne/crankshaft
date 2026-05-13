#ifndef __crankshaftutf8doth__
#define __crankshaftutf8doth__
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Utf8Output {
    int32_t length;
    char chars[4];
};

//Returns the 'length' of the pointed at character. -1 if invalid character
int32_t CS_utf8Length( const char *checkMe );
const struct CS_Utf8Output *CS_utf8FromWin1252( const char *win1252Char );
int32_t CS_utf8FromInt( int32_t codePoint, char *out, int32_t length );
int32_t CS_utf8CountChars( const char *start, const char *end );

#ifdef __cplusplus
}
#endif
#endif
