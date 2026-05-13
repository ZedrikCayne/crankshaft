#ifndef __crankshaftstackdoth__
#define __crankshaftstackdoth__
#include <stdbool.h>
#include <stdint.h>

/********************************************************************
 *
 * Generic stack. Pushes and pops pointers and copies data. All items
 * are assumed to be the same size.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Stack {
    int32_t itemSize;
    int32_t itemsPerSlab;
    int32_t sizePerItem;
    int32_t alignment;
    int32_t current;
    struct CS_Stack *nextStack;
    char *buff;
};

struct CS_Stack *CS_stackAlloc( int32_t sizePerItem, int32_t itemsPerSlab, int32_t itemAlignment );
struct CS_Stack *CS_stackAllocPointer( int32_t itemsPerSlab );
void CS_stackFree( struct CS_Stack *freeMe );

//Generic cases...
bool CS_stackPush( struct CS_Stack *onTo, const void *this );
bool CS_stackPop( struct CS_Stack *offOf, void *that );

//Specific push'n'pop pointers
bool CS_stackPushPointer( struct CS_Stack *onTo, const void *this );
void *CS_stackPopPointer( struct CS_Stack *offOf );

#ifdef __cplusplus
}
#endif
#endif
