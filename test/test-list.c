#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/list.h>

extern bool test_list(void);

static int testCount = 0;
static int testSucceeded = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

static struct CS_List *privateTestListPointersToInt(int nItems) {
    struct CS_List *testList = CS_listCreate(1024);
    if( testList ) {
        for( int i = 0; i < nItems; ++i ) {
            CS_listPushTail( testList, &i, sizeof(int) );
        }
    }
    return testList;
}

static struct CS_List *privateTestListVoidsOfInt(int nItems) {
    struct CS_List *testList = CS_listCreate(1024);
    if( testList ) {
        for( int i = 0; i < nItems; ++i ) {
            CS_listPushTail( testList, (void*)(int)i, 0 );
        }
    }
    return testList;
}

static int ITEM_VAL(const struct  CS_ListItem *item) {
    if( item == NULL ) return -1;

    return (int)(void*)(item->what);
}
#pragma GCC diagnostic pop

static int compareItems( const struct CS_ListItem *left, const struct CS_ListItem *right ) {
    if( ITEM_VAL(left) < ITEM_VAL(right) ) return -1;
    if( ITEM_VAL(left) == ITEM_VAL(right) ) return 0;
    return 1;
}

#define ARRAY_SIZE(__ARRAY) (sizeof(__ARRAY)/sizeof(__ARRAY[0]))
#define GET_AND_CHECK(__INDEX,__EXPECTED) item = CS_listGetByIndex(testList,__INDEX);CS_FAIL_ON_FALSE((ITEM_VAL(item)==(__EXPECTED)),CS_tempBuffSnprintf(64,"Checking index %d at line %d",__INDEX,__LINE__),"Wanted %d but got %d",__EXPECTED,ITEM_VAL(item))
#define VERIFY_LENGTH(__EXPECTED)  CS_FAIL_ON_FALSE( CS_listCount( testList ) == __EXPECTED, CS_tempBuffSnprintf( 64, "List count matches %d", __EXPECTED ), "List count actual: %d", CS_listCount( testList ) );

bool test_list(void) {
    //Tests go here:
    
    struct CS_List *testList = privateTestListPointersToInt(10);

    CS_FAIL_ON_NULL( testList, "Initial test list.", "Got NULL" );
    if( testList ) {

        VERIFY_LENGTH( 10 );
        for( int i = 0; i < 10; ++i ) {
            const struct CS_ListItem *item = CS_listGetByIndex( testList, i );
            CS_FAIL_ON_NULL( item, CS_tempBuffSnprintf( 64, "Checking %d", i ), "Got NULL" );
            if( item ) CS_FAIL_ON_FALSE( i == *(int*)item->what, CS_tempBuffSnprintf( 64, "Checking value of %d", i ), "Got %d", *(int*)item->what );
            int negativeIndex = -1 - i;
            int valueThatShouldBeThere = 9 - i;
            item = CS_listGetByIndex( testList, negativeIndex );
            CS_FAIL_ON_NULL( item, CS_tempBuffSnprintf( 64, "Checking %d", negativeIndex ), "Got NULL" );
            if( item ) CS_FAIL_ON_FALSE( valueThatShouldBeThere == *(int*)item->what, CS_tempBuffSnprintf( 64, "Checking value of %d", negativeIndex), "Wanted %d Got %d", valueThatShouldBeThere, *(int*)item->what );
        }
        CS_listDestroy( testList );
    }

    testList = privateTestListVoidsOfInt(5);
    //0,1,2,3,4

    CS_FAIL_ON_NULL( testList, "Test list 2 fail to allocate.", "Got NULL" );
    if( testList ) {
        const struct CS_ListItem *item;
        GET_AND_CHECK(1,1);
        if( item ) CS_listPushBefore( testList, item, (void*)(int)99, 0 );
        //0,99,1,2,3,4
        VERIFY_LENGTH(6);
        GET_AND_CHECK(1,99);
        GET_AND_CHECK(2,1);
        if( item ) CS_listPushAfter( testList, item, (void*)(int)35, 0 );
        //0,99,1,35,2,3,4
        VERIFY_LENGTH(7);
        GET_AND_CHECK(3,35);
        GET_AND_CHECK(2,1);
        GET_AND_CHECK(4,2);
        GET_AND_CHECK(-3,2);
        GET_AND_CHECK(-4,35);
        GET_AND_CHECK(-2,3);
        if( item ) CS_listPushBefore( testList, item, (void*)(int)12, 0 );
        //0,99,1,35,2,12,3,4
        VERIFY_LENGTH(8);
        GET_AND_CHECK(-3,12);
        GET_AND_CHECK(-1,4);
        if( item ) CS_listPushAfter( testList, item, (void*)(int)17, 0 );
        //0,99,1,35,2,12,3,4,17
        VERIFY_LENGTH(9);
        GET_AND_CHECK(-1,17);
        GET_AND_CHECK(0,0);
        if( item ) CS_listPushBefore( testList, item, (void*)(int) 55, 0 );
        //55,0,99,1,35,2,12,3,4,17
        VERIFY_LENGTH(10);
        item = CS_listGetByIndex( testList, 0 );
        GET_AND_CHECK(0,55);
        if( item ) CS_listPushAfter( testList, item, (void*)(int)31, 0 );
        //55,31,0,99,1,35,2,12,3,4,17
        VERIFY_LENGTH(11);
        GET_AND_CHECK(0,55);
        GET_AND_CHECK(1,31);
        item = CS_listGetByIndex( testList, -1 );
        GET_AND_CHECK(-1,17);
        if( item ) CS_listPushBefore( testList, item, (void*)(int)11, 0 );
        //55,31,0,99,1,35,2,12,3,4,11,17
        VERIFY_LENGTH(12);
        GET_AND_CHECK(-2,11);
        GET_AND_CHECK(-1,17);

        int values[] =       {55,31, 0,99, 1,35, 2,12, 3, 4,11,17};
        int valuesSorted[] = { 0, 1, 2, 3, 4,11,12,17,31,35,55,99};
        item = CS_listGetHead( testList );
        for( int i = 0; i < ARRAY_SIZE(values); ++i ) {
            CS_FAIL_ON_FALSE( ITEM_VAL(item) == values[i], CS_tempBuffSnprintf( 64, "Checking index forward %d", i), "Wanted %d but got %d", values[i], ITEM_VAL(item) );
            item = item->next;
        }
        CS_FAIL_ON_NOT_NULL( item, "We should have hit the end of the list by now.", "Item still available value %d", ITEM_VAL(item) );
        item = CS_listGetTail( testList );
        for( int i = 0; i < ARRAY_SIZE(values); ++i ) {
            CS_FAIL_ON_FALSE( ITEM_VAL(item) == values[ARRAY_SIZE(values) - 1 - i], CS_tempBuffSnprintf( 64, "Checking index backward %d", i), "Wanted %d but got %d", values[ARRAY_SIZE(values) - 1 - i], ITEM_VAL(item) );
            item = item->last;
        }
        CS_FAIL_ON_NOT_NULL( item, "We should have hit the front of the list by now.", "Item still available value %d", ITEM_VAL(item) );

        int currentIndex = 0;
        CS_LIST_ITER( testList, theItem ) {
            CS_FAIL_ON_FALSE( ITEM_VAL(theItem)==values[currentIndex], CS_tempBuffSnprintf( 64, "Forward Iteration Check index %d", currentIndex ), "Wanted %d but got %d", values[currentIndex], ITEM_VAL(theItem) );
            ++currentIndex;
        }
        CS_LIST_ITER_REVERSE( testList, theItem ) {
            --currentIndex;
            CS_FAIL_ON_FALSE( ITEM_VAL(theItem)==values[currentIndex], CS_tempBuffSnprintf( 64, "Forward Iteration Check index %d", currentIndex ), "Wanted %d but got %d", values[currentIndex], ITEM_VAL(theItem) );
        }

        CS_listSort( testList, compareItems );
        currentIndex = 0;
        CS_LIST_ITER( testList, theItem ) {
            CS_FAIL_ON_FALSE( ITEM_VAL(theItem)==valuesSorted[currentIndex], CS_tempBuffSnprintf( 64, "Forward Iteration Check index %d", currentIndex ), "Wanted %d but got %d", valuesSorted[currentIndex], ITEM_VAL(theItem) );
            ++currentIndex;
        }

        for( int i = 0; i < ARRAY_SIZE( valuesSorted ); ++i ) {
            const struct CS_ListItem * poppedItem = CS_listPopHead(testList);
            CS_FAIL_ON_NULL( poppedItem, "List should have a popped item.", "We started running out at %i items", i + 1 );
            if( poppedItem ) {
                CS_FAIL_ON_FALSE( ITEM_VAL( poppedItem ) == valuesSorted[i], "Checking popped item order.", "Failed on index %d", i );
            } else {
                break;
            }
        }
        
        VERIFY_LENGTH( 0 );

        CS_listDestroy( testList );
    }
    return testCount !=
           testSucceeded;
}


