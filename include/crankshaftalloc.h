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
#define CS_allocDuplicate(_X,_Y) CS_allocDuplicateMalloc(_X,_Y)
#define CS_allocB(_B,_X) malloc(_X)
#define CS_freeB(_B,_X) free(_X)
#define CS_reallocB(_B,_X,_Y) realloc(_X,_Y)
#define CS_allocDuplicateB(_B,_X,_Y) CS_allocDuplicateMalloc(_X,_Y)
#define CS_allocZeor(_X) CS_allocZeroMalloc(_X)

#else

void CS_setFailAlloc(int percentageOfTheTime);
void CS_setMaxAlloc(int maxSize);

#define CS_alloc(X) CS_alloc_detailled(X,__FILE__,__LINE__)
#define CS_free(X) CS_free_detailled(X,__FILE__,__LINE__)
#define CS_realloc(X,Y) CS_realloc_detailled(X,Y,__FILE__,__LINE__)
#define CS_allocDuplicate(_X,_Y) CS_allocDuplicate_detailled(_X,_Y,__FILE__,__LINE__)
#define CS_allocZero(_X) CS_allocZero_detailled(_X,__FILE__,__LINE__)
#define CS_allocB(_B,_X) (_B)?malloc(_X):CS_alloc_detailled(_X,__FILE__,__LINE__)
#define CS_freeB(_B,_X) (_B)?free(_X):CS_free_detailled(_X,__FILE__,__LINE__)
#define CS_reallocB(_B,_X,_Y) (_B)?realloc(_X,_Y):CS_realloc_detailled(_X,_Y,__FILE__,__LINE__)
#define CS_allocDuplicateB(_B,_X,_Y) (_B)?CS_allocDuplicateMalloc(_X,_Y):CS_allocDuplicate_detailled(_X,_Y,__FILE__,__LINE__)
#define CS_allocZeroB(_B,_X) (_B)?CS_allocDuplicateMalloc(_B):CS_allocDuplicate_detailled(_X,__FILE__,__LINE__)

void *CS_alloc_detailled(unsigned long size,const char *file,int line);
void CS_free_detailled(void *freeMe,const char *file, int line);
void *CS_realloc_detailled(void *reallocMe,unsigned long size,const char *file,int line);
void *CS_allocDuplicate_detailled( const void *duplicateMe, unsigned long size, const char *file, int line);
void *CS_allocZero_detailled( unsigned long size, const char *file, int line);

#endif

void *CS_allocDuplicateMalloc(const void *duplicateMe, unsigned long size);
void *CS_allocZeroMalloc(unsigned long size);

#ifdef CS_ALLOC_TRACKING
#ifdef CS_ALLOC_USE_MALLOC
#error "You can't have CS_ALLOC_TRACKING and CS_ALLOC_USE_MALLOC defined at the same time."
#endif
int CS_allocSystemTracker(int concurrentTrackingSlots,unsigned int flags);
int CS_allocSystemTrackerKill();
int CS_allocSystemReport();
#else
#define CS_allocSystemTracker(X,Y) 0 
#define CS_allocSystemTrackerKill() 0 
#define CS_allocSystemReport() 
#endif

#define CS_ALLOC_FLAG_LOG_ERRORS        0x00000001
#define CS_ALLOC_FLAG_WARN_LOCALITY     0x00000002
#define CS_ALLOC_FLAG_WARN_REALLOC_FAIL 0x00000004
#define CS_ALLOC_FLAG_ALL               0x00000007

#ifdef __cplusplus
}
#endif
#endif
