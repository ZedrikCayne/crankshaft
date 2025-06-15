#ifndef __crankshaftutildoth__
#define __crankshaftutildoth__
#include <stdbool.h>

/********************************************************************
 *
 * Static alignment bits because we use them sprinkled everywhere
 * in memory allocation bits that get called often enough that
 * the extra calls are/were significant.
 *
 *******************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

__inline__ static int CS_align(int root, int alignment) {
    int mod = root % alignment;
    if( mod == 0 ) return root;
    return root + alignment - mod;
}

__inline__ static void *CS_alignVoid(void *root, int alignment) {
    int mod = ((long long)root) % alignment;
    if( mod == 0 ) return root;
    return (void *)((char*)(root + (alignment - mod)));
}

#ifdef __cplusplus
}
#endif
#endif
