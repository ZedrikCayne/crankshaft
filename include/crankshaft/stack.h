#ifndef __crankshaftstackdoth__
#define __crankshaftstackdoth__
#include <stdbool.h>

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
    int itemSize;
    int itemsPerSlab;
    int sizePerItem;
    int alignment;
    int current;
    struct CS_Stack *nextStack;
    char *buff;
};

struct CS_Stack *CS_stackAlloc( int sizePerItem, int itemsPerSlab, int itemAlignment );
struct CS_Stack *CS_stackAllocPointer( int itemsPerSlab );
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
