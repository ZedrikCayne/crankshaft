#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crankshaft/alloc.h"
#include "crankshaft/logger.h"
#include "crankshaft/stack.h"
#include "crankshaft/util.h"

struct CS_Stack *CS_stackAlloc( int sizePerItem, int itemsPerSlab, int itemAlignment ) {
    struct CS_Stack *returnValue = CS_alloc(sizeof(struct CS_Stack));
    if( !returnValue ) {
        return NULL;
    }
    returnValue->nextStack = NULL;
    returnValue->itemsPerSlab = itemsPerSlab;
    returnValue->itemSize = sizePerItem;
    returnValue->sizePerItem = CS_align( sizePerItem, itemAlignment );
    returnValue->current = 0;
    returnValue->alignment = itemAlignment;
    returnValue->buff = CS_alloc( returnValue->sizePerItem * returnValue->itemsPerSlab );
    if( returnValue->buff == NULL ) {
        CS_free(returnValue);
        returnValue = NULL;
    }
    return returnValue;
}

struct CS_Stack *CS_stackAllocPointer( int itemsPerSlab ) {
    return CS_stackAlloc( sizeof( void * ), itemsPerSlab, sizeof( void * ) );
}

void CS_stackFree( struct CS_Stack *freeMe ) {
    struct CS_Stack *currentStack = freeMe;
    while( currentStack != NULL ) {
        struct CS_Stack *nextStack = currentStack->nextStack;
        if( currentStack->buff ) CS_free(currentStack->buff);
        CS_free(currentStack);
        currentStack = nextStack;
    }
}

//Generic cases...
bool CS_stackPush( struct CS_Stack *onTo, const void *this ) {
    int whichStack = onTo->current / onTo->itemsPerSlab;
    int whichOne = onTo->current % onTo->itemsPerSlab;

    struct CS_Stack *stack = onTo;
    for( int i = 0; i < whichStack; ++i ) {
        if( stack->nextStack == NULL ) {
            stack->nextStack = CS_stackAlloc( onTo->itemSize, onTo->itemsPerSlab, onTo->alignment );
            if( stack->nextStack == NULL )
                return true;
        }
        stack = stack->nextStack;
    }

    void *copyTo = stack->buff + ( whichOne * onTo->sizePerItem );
    memcpy( copyTo, this, onTo->itemSize );
    ++onTo->current;
    return false;
}

bool CS_stackPop( struct CS_Stack *offOf, void *that ) {
    if( offOf->current == 0 ) return true;
    --offOf->current;
    int whichStack = offOf->current / offOf->itemsPerSlab;
    int whichOne = offOf->current % offOf->itemsPerSlab;

    struct CS_Stack *stack = offOf;
    for( int i = 0; i < whichStack; ++i ) {
        stack = stack->nextStack;
    }
    void *copyFrom = stack->buff + ( whichOne * offOf->sizePerItem );
    memcpy( that, copyFrom, offOf->itemSize );
    return false;
}

//Specific push'n'pop pointers
bool CS_stackPushPointer( struct CS_Stack *onTo, const void *this ) {
    int whichStack = onTo->current / onTo->itemsPerSlab;
    int whichOne = onTo->current % onTo->itemsPerSlab;

    struct CS_Stack *stack = onTo;
    for( int i = 0; i < whichStack; ++i ) {
        if( stack->nextStack == NULL ) {
            stack->nextStack = CS_stackAlloc( onTo->itemSize, onTo->itemsPerSlab, onTo->alignment );
            if( stack->nextStack == NULL )
                return true;
        }
        stack = stack->nextStack;
    }

    ((void **)stack->buff)[whichOne] = (void*)this;
    ++onTo->current;
    return false;
}

void *CS_stackPopPointer( struct CS_Stack *offOf ) {
    if( offOf->current == 0 ) return NULL;
    --offOf->current;
    int whichStack = offOf->current / offOf->itemsPerSlab;
    int whichOne = offOf->current % offOf->itemsPerSlab;

    struct CS_Stack *stack = offOf;
    for( int i = 0; i < whichStack; ++i ) {
        stack = stack->nextStack;
    }
    return ((void **)stack->buff)[whichOne];
}



