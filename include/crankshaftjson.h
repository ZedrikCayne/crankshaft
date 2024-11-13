#ifndef __crankshaftjsondoth__
#define __crankshaftjsondoth__
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "crankshaftstringbuilder.h"

/******************************************************************************
 *
 * Json handling. We can pull stuff in in a couple of ways. 
 *
 * 1) In place, assuming we pulled in all the json in one memory blob,
 *    all the pointers to values will be in that blob. Good for short
 *    term stuff. All the linear allocator is used for is the json structure.
 *    Generally if the entire json fits in the receive buffer of the client
 *    socket the strings will be in place there and may be transformed to
 *    unquoted strings for processing in place. This is the unsafe but
 *    probably fast option.
 *
 * 2) Copy of data. All values will be copied into the linear allocator
 *    as we go, should be used when we are dealing with json that is
 *    being streamed in and stuff that is not transient. Generally
 *    if the incoming json is big, or is expected to live past the current
 *    length of the client receive buffer. This is the safe option always.
 *
 * Json will sit in QUOTED and *_AS_STRING data types unless otherwise
 * transformed. No string will be split across slabs of the allocator,
 * but if strings of abnormally large sized are required, one will be 
 * allocated with cs_alloc and be CS_JSON_STRING_QUOTED_ALLOCATED.
 *
 * As for transformations, you can guarantee you can go from QUOTED to 
 * UNQUOTED in place. Going from UNQUOTED to QUOTED may take up to 6x
 * the original space so values may go from CS_JSON_STRING_UNQUOTED to
 * CS_JSON_STRING_QUOTED_ALLOCATED on quoting for transmission.
 *
 * (If your entire data set is bytes from 0 to 31 exclusive of backspace (8),
 * horizontal tab (9), form feed (14), carriage return (13) or line feed (10)
 * it will expand out to six bytes \uXXXX. Highly unlikely but you never know.
 *
 * UNQUOTED strings are assumed to be UTF-8. But we don't actually check
 * the formatting. Be aware. If you're not careful this will mean that
 * the receiver will get something...mangled.
 * 
 * STRUCTURE:
 * Top Most Node is a CS_JsonType. name will be NULL, up will be NULL.
 * There will be a linear allocator. It's type will be highly depentent
 * on the incoming json. It may be a naked value, (integer, string, number)
 * or a container (Array or Object) sibling nodes are in 'next' and 'last'
 * pointer. 'up' points at the parent. For example:
 *
 * Say this is in the buffer you want to deserialize.
 *
 * {"type":"database",
 *  "contents":[{"id":0,"name":"Foo"},
 *              {"id":1,"name":"Bar"},
 *              {"id":2,"name":"Baz","pages":[1,3,5,2.3]}]}
 *
 * We're going to do this in place.
 *
 * {"type*:"database*,
 *  "contents*:[{"id*:0*"name*:"Foo*},
 *              {"id*:1*"name*:"Bar*},
 *              {"id*:2*"name*:"Baz*,"pages*:[1*3*5*2.3*]}]}
 *
 * Notice all numbers are just strings right now. And you will get the below
 * structure (pointer to #0)
 *                  name,       up,     next,   last, type,                       size, union,      allocator, alloc
 *  #0 CS_JsonNode( NULL,       NULL,   NULL,   NULL, CS_JSON_OBJECT,               2,  #1,         allocator, NULL )
 *  #1 CS_JsonNode( "type",     #0,     #2,     NULL, CS_JSON_STRING_UNQUOTED,      8,  "database", allocator, NULL )
 *  #2 CS_JsonNode( "contents", #0,     NULL,   NULL, CS_JSON_ARRAY,                3,  #3,         allocator, NULL )
 *  #3 CS_JsonNode( NULL,       #2,     #6,     NULL, CS_JSON_OBJECT,               2,  #4,         allocator, NULL )
 *  #4 CS_JsonNode( "id",       #3,     #5,     NULL, CS_JSON_INTEGER_AS_STRING,    1,  "0",        allocator, NULL )
 *  #5 CS_JsonNode( "name",     #3,     NULL,   #4,   CS_JSON_STRING_UNQUOTED,      3,  "Foo",      allocator, NULL )
 *  #6 CS_JsonNode( NULL,       #2,     #9,     #3,   CS_JSON_OBJECT,               2,  #7,         allocator, NULL )
 *  #7 CS_JsonNode( "id",       #6,     #8,     NULL, CS_JSON_INTEGER_AS_STRING,    1,  "1",        allocator, NULL )
 *  #8 CS_JsonNode( "name",     #6,     NULL,   #7,   CS_JSON_STRING_UNQUOTED,      3,  "Bar",      allocator, NULL )
 *  #9 CS_JsonNode( NULL,       #2,     NULL,   #6,   CS_JSON_OBJECT,               3,  #10,        allocator, NULL )
 * #10 CS_JsonNode( "id",       #9,     #11,    NULL, CS_JSON_INTEGER_AS_STRING,    1,  "2",        allocator, NULL )
 * #11 CS_JsonNode( "name",     #9,     #12,    #10,  CS_JSON_STRING_UNQUOTED,      3,  "Baz",      allocator, NULL )
 * #12 CS_JsonNode( "pages",    #9,     #9,     #11,  CS_JSON_ARRAY,                4,  #13,        allocator, NULL )
 * #13 CS_JsonNode( NULL,       #12,    #14,    NULL, CS_JSON_INTEGER_AS_STRING,    1,  "1",        allocator, NULL )
 * #14 CS_JsonNode( NULL,       #12,    #15,    #13,  CS_JSON_INTEGER_AS_STRING,    1,  "3",        allocator, NULL )
 * #15 CS_JsonNode( NULL,       #12,    #16,    #14,  CS_JSON_INTEGER_AS_STRING,    1,  "5",        allocator, NULL )
 * #16 CS_JsonNode( NULL,       #12,    NULL,   #15,  CS_JSON_FLOAT_AS_STRING,      3,  "2.3",      allocator, NULL )
 *
 * Depending on what you want/need, you can have it transform everything to
 * local binary (Unquoting json to UTF-8, numbers from strings to long long or
 * double precision floats)
 *
 * Everything is allocated inside the linear allocator pointed at by every node.
 * CS_freeJson() will cycle up to the root node of whatever structure you are in
 * blast through it freeing any 'allocated' bits, (the last dangling pointer)
 * and then finally freeing the linear allocator.
 *
 * This means you can modify a CS_JsonNode in place safely. Anything new gets
 * tacked onto the end of the linear allocator.
 *
 * All the modification and creation assume that all const char * inputs are
 * valid UTF-8. We don't actually check and we're reasonably confident that
 * we're handling all the edge cases correctly, but tag a test into 
 * test/test-json.c if you've got concerns about particular cases.
 *
 * (make test comes out clean)
 *
 *****************************************************************************/

enum CS_JsonType {
    CS_JSON_ARRAY,
    CS_JSON_OBJECT,
    CS_JSON_STRING_QUOTED,
    CS_JSON_STRING_UNQUOTED,
    CS_JSON_STRING_QUOTED_ALLOCATED,
    CS_JSON_STRING_UNQUOTED_ALLOCATED,
    CS_JSON_INTEGER,
    CS_JSON_INTEGER_AS_STRING,
    CS_JSON_FLOAT,
    CS_JSON_FLOAT_AS_STRING,
    CS_JSON_true,
    CS_JSON_false,
    CS_JSON_null,
    CS_JSON_UNKNOWN,
    CS_JSON_ERROR
};

struct CS_JsonNode {
    const char *name;
    struct CS_JsonNode *up;
    struct CS_JsonNode *next;
    struct CS_JsonNode *last;
    unsigned long typeEnum;
    unsigned long nItemsOrLength;
    union {
        const char *stringValue;
        long long intValue;
        double floatValue;
        struct CS_JsonNode *container;
    };
    void *voidLinearAllocator;
    void *alloc;
};

struct CS_JsonNode *CS_parseJsonCopy(const char *source, int inputLength, int allocSize);
struct CS_JsonNode *CS_parseJson(const char *source, int inputLength, int allocSize);
void CS_freeJson( struct CS_JsonNode *any );
bool CS_jsonNodesEquivalent(struct CS_JsonNode *a, struct CS_JsonNode *b);
struct CS_JsonNode *CS_jsonNodeToUnquoted( struct CS_JsonNode *in, bool followTree );

struct CS_StringBuilder *CS_jsonNodePrintable(const struct CS_JsonNode *printMe);

bool CS_unquoteInPlace(char *inputString, int len);

struct CS_StringBuilder *CS_quoteStringToStringBuilder(const char *inputString, int len, struct CS_StringBuilder *out);
struct CS_StringBuilder *CS_quoteString(const char *inputString, int len);
struct CS_StringBuilder *CS_unquoteString(const char *inputString, int len);

const char *CS_jsonEnumTypeAsString(const int enumType);

/****************************************************************************
 * Comparison by value. We're mostly using it in our tests to make sure when
 * we deserialize by copy/non copy things come up identical. Not recommended
 * for production anything.
 ****************************************************************************/
bool CS_jsonNodesEquivalent(struct CS_JsonNode *a, struct CS_JsonNode *b);

struct CS_JsonNode *CS_jsonNodeNew( int allocSize );
/******************************************************
 *
 * Convenience functions for modifying stuff in place
 *
 ******************************************************/
struct CS_JsonNode *CS_jsonNodeAppendUnquotedString( struct CS_JsonNode *appendTo, const char *name, const char *value );
struct CS_JsonNode *CS_jsonNodeAppendFloat( struct CS_JsonNode *appendTo, const char *name, double value );
struct CS_JsonNode *CS_jsonNodeAppendInteger( struct CS_JsonNode *appendTo, const char *name, long long value );
struct CS_JsonNode *CS_jsonNodeAppendObject( struct CS_JsonNode *appendTo, const char *name );
struct CS_JsonNode *CS_jsonNodeAppendArray( struct CS_JsonNode *appendTo, const char *name );
struct CS_JsonNode *CS_jsonNodeAddUnquotedString( struct CS_JsonNode *addTo, const char *name, const char *value );
struct CS_JsonNode *CS_jsonNodeAddFloat( struct CS_JsonNode *addTo, const char *name, double value );
struct CS_JsonNode *CS_jsonNodeAddInteger( struct CS_JsonNode *addTo, const char *name, long long value );
struct CS_JsonNode *CS_jsonNodeAddObject( struct CS_JsonNode *addTo, const char *name );
struct CS_JsonNode *CS_jsonNodeAddArray( struct CS_JsonNode *addTo, const char *name );
struct CS_JsonNode *CS_jsonNodeRemoveNode( struct CS_JsonNode *remove ); //Note, you cannot remove the root node.

/*********************************************************************
 *
 * Warning about these, you can generate invalid json with them.
 * Use with caution but I'm not your responsible adult.
 *
 *********************************************************************/
struct CS_JsonNode *CS_jsonNodeAppendFloatAsString( struct CS_JsonNode *appendTo, const char *name, const char *value );
struct CS_JsonNode *CS_jsonNodeAppendIntegerAsString( struct CS_JsonNode *appendTo, const char *name, const char *value );
struct CS_JsonNode *CS_jsonNodeAppendQuotedString( struct CS_JsonNode *appendTo, const char *name, const char *value );

#ifdef __cplusplus
}
#endif
#endif
