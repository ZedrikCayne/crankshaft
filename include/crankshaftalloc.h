#ifndef __crankshaftallocdoth__
#define __crankshaftallocdoth__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#ifdef CS_ALLOC_USE_MALLOC

#define CS_setFailAlloc(X)
#define CS_setMaxAlloc(X)

#define CS_alloc(X) malloc(X)
#define CS_free(X) free(X)
#define CS_realloc(X,Y) realloc(X,Y)

#else

void CS_setFailAlloc(int percentageOfTheTime);
void CS_setMaxAlloc(int maxSize);

#define CS_alloc(X) CS_alloc_detailled(X,__FILE__,__LINE__)
#define CS_free(X) CS_free_detailled(X,__FILE__,__LINE__)
#define CS_realloc(X,Y) CS_realloc_detailled(X,Y,__FILE__,__LINE__)

void *CS_alloc_detailled(unsigned long size,const char *file,int line);
void CS_free_detailled(void *freeMe,const char *file, int line);
void *CS_realloc_detailled(void *reallocMe,unsigned long size,const char *file,int line);

#endif

#ifdef __cplusplus
}
#endif
#endif
