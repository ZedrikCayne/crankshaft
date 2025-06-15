#ifndef __crankshaftlistdoth__
#define __crankshaftlistdoth__
#include <stdbool.h>

#include <crankshaft/linearalloc.h>

/********************************************************************
 *
 * CS_List
 *
 * Growable list. Based on a linear allocator. Copies all data
 * in. Returns static *items.
 *
 ********************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

struct CS_ListItem {
    const void *what;
    const struct CS_ListItem *next;
    const struct CS_ListItem *last;
    const int size;
};

struct CS_List;

struct CS_List *CS_listCreate( int initialAllocSizeInBytes );
bool CS_listDestroy( struct CS_List *list );
bool CS_listRemove( struct CS_List *list, const struct CS_ListItem *item );
bool CS_listReset( struct CS_List *list );
bool CS_listPushHead( struct CS_List *list, const void *what, int size );
bool CS_listPushTail( struct CS_List *list, const void *what, int size );
bool CS_listPushAfter( struct CS_List *list, const struct CS_ListItem *item, const void *what, int size );
bool CS_listPushBefore( struct CS_List *list, const struct CS_ListItem *item, const void *what, int size );
const struct CS_ListItem *CS_listGetByIndex( struct CS_List *list, int index );
const struct CS_ListItem *CS_listGetHead( struct CS_List *head );
const struct CS_ListItem *CS_listGetTail( struct CS_List *head );

#define CS_LIST_ITER(_LIST,_ITER) for(const struct CS_ListItem* _ITER = CS_listGetHead(_LIST);_ITER!=NULL;_ITER=_ITER->next)
#define CS_LIST_ITER_REVERSE(_LIST,_ITER) for(const struct CS_ListItem* _ITER = CS_listGetTail(_LIST);_ITER!=NULL;_ITER=_ITER->last)

#ifdef __cplusplus
}
#endif
#endif
