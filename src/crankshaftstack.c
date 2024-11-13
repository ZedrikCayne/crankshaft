#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshaftstack.h"
#include "crankshaftutil.h"

struct CS_Stack *CS_Stack_alloc( int sizePerItem, int itemsPerSlab, int itemAlignment ) {
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

struct CS_Stack *CS_Stack_allocPointer( int itemsPerSlab ) {
    return CS_Stack_alloc( sizeof( void * ), itemsPerSlab, sizeof( void * ) );
}

void CS_Stack_free( struct CS_Stack *freeMe ) {
    struct CS_Stack *currentStack = freeMe;
    while( currentStack != NULL ) {
        struct CS_Stack *nextStack = currentStack->nextStack;
        if( currentStack->buff ) CS_free(currentStack->buff);
        CS_free(currentStack);
        currentStack = nextStack;
    }
}

//Generic cases...
bool CS_push( struct CS_Stack *onTo, const void *this ) {
    int whichStack = onTo->current / onTo->itemsPerSlab;
    int whichOne = onTo->current % onTo->itemsPerSlab;

    struct CS_Stack *stack = onTo;
    for( int i = 0; i < whichStack; ++i ) {
        if( stack->nextStack == NULL ) {
            stack->nextStack = CS_Stack_alloc( onTo->itemSize, onTo->itemsPerSlab, onTo->alignment );
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

bool CS_pop( struct CS_Stack *offOf, void *that ) {
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
bool CS_pushPointer( struct CS_Stack *onTo, const void *this ) {
    int whichStack = onTo->current / onTo->itemsPerSlab;
    int whichOne = onTo->current % onTo->itemsPerSlab;

    struct CS_Stack *stack = onTo;
    for( int i = 0; i < whichStack; ++i ) {
        if( stack->nextStack == NULL ) {
            stack->nextStack = CS_Stack_alloc( onTo->itemSize, onTo->itemsPerSlab, onTo->alignment );
            if( stack->nextStack == NULL )
                return true;
        }
        stack = stack->nextStack;
    }

    ((void **)stack->buff)[whichOne] = (void*)this;
    ++onTo->current;
    return false;
}

void *CS_popPointer( struct CS_Stack *offOf ) {
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



