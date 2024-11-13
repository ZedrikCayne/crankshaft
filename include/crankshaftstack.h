#ifndef __crankshaftstackdoth__
#define __crankshaftstackdoth__
#include <stdbool.h>

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

struct CS_Stack *CS_Stack_alloc( int sizePerItem, int itemsPerSlab, int itemAlignment );
struct CS_Stack *CS_Stack_allocPointer( int itemsPerSlab );
void CS_Stack_free( struct CS_Stack *freeMe );

//Generic cases...
bool CS_push( struct CS_Stack *onTo, const void *this );
bool CS_pop( struct CS_Stack *offOf, void *that );

//Specific push'n'pop pointers
bool CS_pushPointer( struct CS_Stack *onTo, const void *this );
void *CS_popPointer( struct CS_Stack *offOf );

#ifdef __cplusplus
}
#endif
#endif
