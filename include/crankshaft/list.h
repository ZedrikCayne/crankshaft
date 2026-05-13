#ifndef __crankshaftlistdoth__
#define __crankshaftlistdoth__
#include <stdbool.h>
#include <stdint.h>

/********************************************************************
 *
 * CS_List
 *
 * Growable list. Based on a linear allocator. Copies all data
 * in. Returns static *items. Warning, will forever grow.
 *
 ********************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

struct CS_ListItem {
    const struct CS_ListItem *next;
    const struct CS_ListItem *last;
    const void *what;
    const int32_t size;
};

struct CS_List;

struct CS_List *CS_listCreate( int32_t initialSize );
bool CS_listDestroy( struct CS_List *list );
bool CS_listRemove( struct CS_List *list, const struct CS_ListItem *item );
bool CS_listReset( struct CS_List *list );
bool CS_listPushHead( struct CS_List *list, const void *what, int32_t size );
bool CS_listPushTail( struct CS_List *list, const void *what, int32_t size );
bool CS_listPushAfter( struct CS_List *list, const struct CS_ListItem *item, const void *what, int32_t size );
bool CS_listPushBefore( struct CS_List *list, const struct CS_ListItem *item, const void *what, int32_t size );
const struct CS_ListItem *CS_listPopHead( struct CS_List *list );
const struct CS_ListItem *CS_ListPopTail( struct CS_List *list );
const struct CS_ListItem *CS_listGetByIndex( struct CS_List *list, int32_t index );
const struct CS_ListItem *CS_listGetHead( struct CS_List *list );
const struct CS_ListItem *CS_listGetTail( struct CS_List *list );
bool CS_listReturnItem( struct CS_List *list, const struct CS_ListItem *item );
int32_t CS_listCount( struct CS_List *list );
void CS_listSort( struct CS_List *list, int32_t (*compare)( const struct CS_ListItem *left, const struct CS_ListItem *right ) );

#define CS_LIST_ITER(_LIST,_ITER) for(const struct CS_ListItem* _ITER = CS_listGetHead(_LIST);_ITER!=NULL;_ITER=_ITER->next)
#define CS_LIST_ITER_REVERSE(_LIST,_ITER) for(const struct CS_ListItem* _ITER = CS_listGetTail(_LIST);_ITER!=NULL;_ITER=_ITER->last)

#ifdef __cplusplus
}
#endif
#endif
