#ifndef __crankshaftutildoth__
#define __crankshaftutildoth__
#include <stdbool.h>

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
