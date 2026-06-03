#ifndef __crankshaftlinearallocdoth__
#define __crankshaftlinearallocdoth__
#include <stdbool.h>
#include <stdint.h>

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

struct CS_LinearAllocator;

void *CS_linearTake(struct CS_LinearAllocator *linearAllocator, int32_t size, int32_t alignment );
void *CS_linearTakeZero(struct CS_LinearAllocator *linearAllocator, int32_t size, int32_t alignment );
char *CS_linearCopyCstring(struct CS_LinearAllocator *linearAllocator, const char *string );
void CS_linearReset(struct CS_LinearAllocator *linearAllocator);

struct CS_LinearAllocator *CS_linearInit( int32_t size );
struct CS_LinearAllocator *CS_linearInitNonGrowable( int32_t size );
void CS_linearFree( struct CS_LinearAllocator *voidAllocator );

#ifdef __cplusplus
}
#endif
#endif
