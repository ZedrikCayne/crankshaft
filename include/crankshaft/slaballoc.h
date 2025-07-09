#ifndef __crankshaftslaballocdoth__
#define __crankshaftslaballocdoth__

/**************************************
 * 
 * Growbale slab allocator.
 *
 * Starts with a big block of count items
 * of size x with new ones aligned at
 * 'alignment'
 *
 * If you exceed the initial, allocates
 * a new block internally.
 *
 * Allocations/frees are O(1) provided
 * you pick reasonable defaults for your
 * initial request.
 *
 **************************************/

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_SlabAllocator;

#define CRANKSHAFT_SLAB_NAME_MAX 20
#define CRANKSHAFT_MIN_ALIGNMENT sizeof(void*)

void *CS_slabTake(struct CS_SlabAllocator *slab);
void *CS_slabTakeZero(struct CS_SlabAllocator *slab);
void *CS_slabTakeCopy(struct CS_SlabAllocator *slab, const void *source);
bool CS_slabReturn(struct CS_SlabAllocator *slab, void *toReturn);

struct CS_SlabAllocator *CS_slabInit( const char *name, int size, int count, int alignment );
struct CS_SlabAllocator *CS_slabInitMalloc( const char *name, int size, int count, int alignment );
bool CS_slabFree( struct CS_SlabAllocator *allocation );
bool CS_slabReset( struct CS_SlabAllocator *allocation );

const char *CS_slabDesc( struct CS_SlabAllocator *allocation );

#ifdef __cplusplus
}
#endif
#endif
