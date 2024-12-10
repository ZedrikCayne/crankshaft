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

#define CRANKSHAFT_SLAB_NAME_MAX 20
#define CRANKSHAFT_MIN_ALIGNMENT 4

void *CS_slabTake(void *slab);
bool CS_slabReturn(void *slab, void *toReturn);

void *CS_slabInit( const char *name, int size, int count, int alignment );
bool CS_slabFree( void *allocation );

const char *CS_slabDesc( void *allocation );

#ifdef __cplusplus
}
#endif
#endif
