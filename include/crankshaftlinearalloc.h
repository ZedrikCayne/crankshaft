#ifndef __crankshaftlinearallocdoth__
#define __crankshaftlinearallocdoth__
#include <stdbool.h>

/**********************************************
 *
 * Linear allocator. Starts with 'size' bytes
 * allocates in a straight line. No freeing.
 *
 * Although you may 'reset' the buffer back to
 * 0. This won't free anything. Not thread
 * safe.
 *
 * Returns NULL if you request an amount bigger
 * than the initial alloc, or on system OOM.
 *
 **********************************************/
#ifdef __cplusplus
extern "C" {
#endif

void *CS_linearTake(void *voidAllocator, int size, int alignment );
void *CS_linearTakeZero( void *linearAllocator, int size, int alignment );
void CS_linearReset(void *voidAllocator);

void *CS_linearInit( int size );
void CS_linearFree( void *voidAllocator );

#ifdef __cplusplus
}
#endif
#endif
