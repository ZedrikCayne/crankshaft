#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/slaballoc.h>

#include <crankshaft/list.h>

struct CS_List {
    struct CS_SlabAllocator *slabAllocator;
    struct ListItem *head;
    struct ListItem *tail;
    int count;
};

struct ListItem {
    struct ListItem *next;
    struct ListItem *prev;
    const void *what;
    int size;
};

struct CS_List *CS_listCreate( int initialSize ) {
    struct CS_List *returnValue = CS_allocZero( sizeof( struct CS_List ) );
    if( !returnValue ) {
        CS_LOG_ERROR( "OOM creating a list." );
        return NULL;
    }
    returnValue->slabAllocator = CS_slabInit( "LIST", sizeof( struct ListItem ), initialSize, sizeof( void *) );
    
    return returnValue;
}

bool CS_listDestroy( struct CS_List *list ) {
    CS_listReset( list );
    CS_slabFree( list->slabAllocator );
    list->slabAllocator = NULL;
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    CS_free( list );
    return false;
}

bool CS_listReset( struct CS_List *list ) {
    struct ListItem *current = list->head;
    //Free the stuff
    while( current ) {
        if( current->what && current->size ) CS_free( (void*)current->what );
        current = current->next;
    }
    //Crush the list
    CS_slabReset( list->slabAllocator );
    list->head = 0;
    list->tail = 0;
    list->count = 0;
    return false;
}


bool privateRemove( struct CS_List *list, struct ListItem *item) {
    if( item == NULL ) return true;
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
    list->count--;
    item->prev = NULL;
    item->next = NULL;
    return false;
}

void privateReturnListItem( struct CS_List *list, struct ListItem *toReturn ) {
    if( toReturn->size && toReturn->what ) {
        CS_free( (void*)toReturn->what );
    }
    toReturn->what = NULL;
    toReturn->size = 0;
    CS_slabReturn( list->slabAllocator, toReturn );
}

struct ListItem *privateNewListItem( struct CS_List *list, const void *what, int size ) {
    struct ListItem *newItem = (struct ListItem *)CS_slabTakeZero( list->slabAllocator );
    if( newItem ) {
        newItem->size = size;
        if( size ) {
            void *writeToMe = CS_allocDuplicate( what, size );
            newItem->what = writeToMe;
            if( !writeToMe ) {
                CS_slabReturn( list->slabAllocator, newItem );
                newItem = NULL;
            }
        } else {
            newItem->what = what;
        }
    }
    return newItem;
}

void privateInsertAfter( struct CS_List *list, struct ListItem *what, struct ListItem *after ) {
    struct ListItem *item = after;
    if( item == NULL ) item = list->tail;
    if( item ) {
        what->prev = item;
        what->next = item->next;
        item->next = what;
        if( what->next == NULL ) {
            list->tail = what;
        } else {
            what->next->prev = what;
        }
    } else {
        list->head = what;
        list->tail = what;
        what->prev = NULL;
        what->next = NULL;
    }
    list->count++;
}

void privateInsertBefore( struct CS_List *list, struct ListItem *what, struct ListItem *before ) {
    struct ListItem *item = before;
    if( item == NULL ) item = list->head;
    if( item ) {
        what->prev = item->prev;
        what->next = item;
        item->prev = what;
        if( what->prev == NULL ) {
            list->head = what;
        } else {
            what->prev->next = what;
        }
    } else {
        list->head = what;
        list->tail = what;
        what->prev = NULL;
        what->next = NULL;
    }
    list->count++;
}

bool CS_listRemove( struct CS_List *list, const struct CS_ListItem *const_item ) {
    if( !list || !const_item ) return NULL;
    bool returnValue = privateRemove( list, (struct ListItem *)const_item );
    privateReturnListItem( list, (struct ListItem *)const_item );
    return returnValue;
}

bool CS_listPushHead( struct CS_List *list, const void *what, int size ) {
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        privateInsertBefore(list, returnValue, list->head);
        return false;
    }
    return true;
}

bool CS_listPushTail( struct CS_List *list, const void *what, int size ) {
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        privateInsertAfter( list, returnValue, list->tail );
        return false;
    }
    return true;
}

bool CS_listPushAfter( struct CS_List *list, const struct CS_ListItem *const_item, const void *what, int size ) {
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        privateInsertAfter( list, returnValue, (struct ListItem *)const_item );
        return false;
    }
    return true;
}

bool CS_listPushBefore( struct CS_List *list, const struct CS_ListItem *const_item, const void *what, int size ) {
    struct ListItem *returnValue = privateNewListItem(list,what,size);
    if( returnValue ) {
        privateInsertBefore( list, returnValue, (struct ListItem *)(const_item) );
        return false;
    }
    return true;
}

const struct CS_ListItem *CS_listPopHead( struct CS_List *list ) {
    if( !list || !list->head ) return NULL;

    struct CS_ListItem *returnValue = (struct CS_ListItem *)list->head;
    privateRemove( list, list->head );

    return returnValue;
}

const struct CS_ListItem *CS_listPopTail( struct CS_List *list ) {
    if( !list || !list->tail ) return NULL;

    struct CS_ListItem *returnValue = (struct CS_ListItem *)list->tail;
    privateRemove( list, list->tail );

    return returnValue;
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

int CS_listCount( struct CS_List *list ) {
    return list->count;
}

void CS_listSort( struct CS_List *list, int (*compare)( const struct CS_ListItem *left, const struct CS_ListItem *right ) ) {
    if( list->head == NULL ) return;
    struct ListItem *currentToInsert = list->head;
    struct ListItem *nextToInsert = currentToInsert->next;
    currentToInsert->next = NULL;
    list->tail = currentToInsert;
    list->count = 1;
    while( nextToInsert ) {
        currentToInsert = nextToInsert;
        nextToInsert = currentToInsert->next;
        bool inserted = false;
        currentToInsert->next = NULL;
        currentToInsert->prev = NULL;
        CS_LIST_ITER( list, searchPoint ){
            if( compare( (struct CS_ListItem *)currentToInsert, (struct CS_ListItem *)searchPoint ) <= 0 ) {
                inserted = true;
                privateInsertBefore( list, currentToInsert, (struct ListItem *)searchPoint );
                break;
            }
        }
        if( !inserted ) privateInsertAfter( list, currentToInsert, NULL );
    }
}

bool CS_listReturnItem( struct CS_List *list, const struct CS_ListItem *item ) {
    if( !list || !item ) return true;
    privateReturnListItem( list, (struct ListItem *)item );
    return false;
}
