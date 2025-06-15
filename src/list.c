#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/list.h>

struct CS_List {
    struct CS_LinearAllocator *linearAllocator;
    struct ListItem *head;
    struct ListItem *tail;
};

struct ListItem {
    const void *what;
    struct ListItem *next;
    struct ListItem *prev;
    int size;
};

struct CS_List *CS_listCreate( int initialAllocSizeInBytes ) {
    struct CS_List *returnValue = CS_alloc( sizeof( struct CS_List ) );
    if( !returnValue ) {
        CS_LOG_ERROR( "OOM creating a list." );
        return NULL;
    }
    returnValue->linearAllocator = CS_linearInit( initialAllocSizeInBytes );
    returnValue->head = NULL;
    returnValue->tail = NULL;
    return returnValue;
}

bool CS_listDestroy( struct CS_List *list ) {
    CS_linearFree( list->linearAllocator );
    list->linearAllocator = NULL;
    list->head = NULL;
    list->tail = NULL;
    CS_free( list );
    return false;
}

bool CS_listRemove( struct CS_List *list, const struct CS_ListItem *const_item ) {
    struct ListItem *item = (struct ListItem *)const_item;
    if( item->prev ) {
        item->prev->next = item->next;
    } else {
        list->head = item->next;
    }
    if( item->next ) {
        item->next->prev = item->prev;
    } else {
        list->tail = item->prev;
    }
    return false;
}

bool CS_listReset( struct CS_List *list ) {
    CS_linearReset( list->linearAllocator );
    list->head = NULL;
    list->tail = NULL;
    return false;
}

struct ListItem *privateNewListItem( struct CS_List *list, const void *what, int size ) {
    struct ListItem *newItem = (struct ListItem *)CS_linearTakeZero( list->linearAllocator, sizeof(struct CS_ListItem), sizeof(void*) );
    if( newItem ) {
        newItem->size = size;
        if( size ) {
            void *writeToMe = CS_linearTake( list->linearAllocator, size, sizeof(void*) );
            newItem->what = writeToMe;
            if( newItem->what ) {
                memcpy( writeToMe, what, size );
            } else {
                newItem = NULL;
            }
        } else {
            newItem->what = what;
        }
    }
    return newItem;
}

bool CS_listPushHead( struct CS_List *list, const void *what, int size ) {
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        if( list->head ) {
            list->head->prev = returnValue;
        } else {
            list->tail = returnValue;
        }
        returnValue->next = list->head;
        list->head = returnValue;
        return false;
    }
    return true;
}

bool CS_listPushTail( struct CS_List *list, const void *what, int size ) {
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        if( list->tail ) {
            list->tail->next = returnValue;
        } else {
            list->head = returnValue;
        }
        returnValue->prev = list->tail;
        list->tail = returnValue;
        return false;
    }
    return true;
}

bool CS_listPushAfter( struct CS_List *list, const struct CS_ListItem *const_item, const void *what, int size ) {
    struct ListItem *item = (struct ListItem *)const_item;
    if( item == NULL ) return CS_listPushTail( list, what, size );
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        returnValue->prev = item;
        returnValue->next = item->next;
        item->next = returnValue;
        if( returnValue->next == NULL ) {
            list->tail = returnValue;
        } else {
            returnValue->next->prev = returnValue;
        }
        return false;
    }
    return true;
}

bool CS_listPushBefore( struct CS_List *list, const struct CS_ListItem *const_item, const void *what, int size ) {
    struct ListItem *item = (struct ListItem *)const_item;
    if( item == NULL ) return CS_listPushHead( list, what, size );
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        returnValue->prev = item->prev;
        returnValue->next = item;
        item->prev = returnValue;
        if( returnValue->prev == NULL ) {
            list->head = returnValue;
        } else {
            returnValue->prev->next = returnValue;
        }
        return false;
    }
    return true;
}

const struct CS_ListItem *CS_listGetByIndex( struct CS_List *list, int index ) {
    bool down = (index < 0);
    struct ListItem *item = (struct ListItem *)(down?list->tail:list->head);
    int target = index;
    int start = 0;
    if( down ) {
        target = 0;
        start = index+1;
    }

    for( int i = start; item && i < target; i++ ) {
        item = down?item->prev:item->next;
    }
    return (struct CS_ListItem *)item;
}

const struct CS_ListItem *CS_listGetHead( struct CS_List *list ) {
    return (const struct CS_ListItem *)list->head;
}

const struct CS_ListItem *CS_listGetTail( struct CS_List *list ) {
    return (const struct CS_ListItem *)list->tail;
}

