#ifndef __crankshaftutf8doth__
#define __crankshaftutf8doth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Utf8Output {
    int length;
    char chars[4];
};

//Returns the 'length' of the pointed at character. -1 if invalid character
int CS_utf8Length( const char *checkMe );
const struct CS_Utf8Output *CS_utf8FromWin1252( const char *win1252Char );
int CS_utf8FromInt( int codePoint, char *out, int length );
int CS_utf8CountChars( const char *start, const char *end );

#ifdef __cplusplus
}
#endif
#endif
