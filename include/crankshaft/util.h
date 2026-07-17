#ifndef __crankshaftutildoth__
#define __crankshaftutildoth__
#include <stdbool.h>
#include <stdint.h>

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

__inline__ static int32_t CS_align(int32_t root, int32_t alignment) {
    int32_t mod = root % alignment;
    if( mod == 0 ) return root;
    return root + alignment - mod;
}

__inline__ static void *CS_alignVoid(void *root, int32_t alignment) {
    int32_t mod = ((long long)root) % alignment;
    if( mod == 0 ) return root;
    return (void *)(((char*)root) + (alignment - mod));
}

#define CS_ARRAY_SIZE(__ARRAY) ((sizeof(__ARRAY)/sizeof(__ARRAY[0])))

//Assumes __XMUTEX is a static ptherad_mutex * and __GLOBAL should be volatile because
//of course gcc might compile the second check out.
#define CS_PMUTEX_PROTECT_GLOBAL(__GLOBAL,__XMUTEX) if(!((volatile void *)(__GLOBAL))&&(pthread_mutex_lock(__XMUTEX)==0)&&((((volatile void *)(__GLOBAL))&&pthread_mutex_unlock(__XMUTEX)!=0)||(!((volatile void *)(__GLOBAL)))))

void *CS_utilLoadWholeFile( const char *filename, int32_t *outSize );

#ifdef __cplusplus
}
#endif
#endif
