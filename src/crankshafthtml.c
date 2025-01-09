#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlinearalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttempbuff.h"

#include "crankshafthtml.h"

static int okayTable[] = {
   //NUL   SOH   STX   ETX   EOT   ENQ   ACK   BEL
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   // BS   TAB    LF    VT    FF    CR    SO    SI
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //DLE   DC1   DC2   DC3   DC4   NAK   SYN   ETB
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //CAN    EM   SUB   ESC    FS    GS    RS    US
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //' '     !     "     #     $     %     &     '
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  (     )     *     +     ,     -     .     /
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  0     1     2     3     4     5     6     7
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
   //  8     9     :     ;     <     =     >     ?
    0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  @     A     B     C     D     E     F     G
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   //  H     I     J     K     L     M     N     O
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   //  P     Q     R     S     T     U     V     W
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   //  X     Y     Z     [     \     /     ^     _
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
   //  `     a     b     c     d     e     f     g
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   //  h     i     j     k     l     m     n     o
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   //  p     q     r     s     t     u     v     w
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   //  x     y     z     {     |     }     ~   DEL
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
}; 

static bool privateCheckOk(const char *in) {
    if( *in < 127 && okayTable[(int)*in] == 2 ) return false;
    const char *current = in;
    while( *current ) {
        if( *current < 127 && !okayTable[(int)*current] )
            return false;
        ++current;
    }
    return true;
}

#define NEWNODE(_X) CS_linearTakeZero( _X, sizeof( struct CS_HtmlNode ), sizeof( void * ) )
#define NEWATTRIBUTE(_X) CS_linearTakeZero( _X, sizeof( struct CS_HtmlAttribute ), sizeof( void *) )
#define NEWATTRIBUTEVALUE(_X) CS_linearTakeZero( _X, sizeof( struct CS_HtmlValue ), sizeof( void *) )

static struct CS_HtmlAttribute *privateFindAttribute( struct CS_HtmlNode *node, const char *attributeName, struct CS_HtmlAttribute **prev ) {
    struct CS_HtmlAttribute *previous = NULL;
    struct CS_HtmlAttribute *current = node->attributes;
    while( current ) {
        if( strcmp( current->name, attributeName ) == 0 )
            break;
        previous = current;
        current = current->next;
    }
    if( prev ) *prev = previous;
    return current;
}

static struct CS_HtmlAttribute *privateAddAttribute( struct CS_HtmlNode *node, const char *attributeName, const char *attributeValue ) {
    struct CS_HtmlAttribute *newAttribute = privateFindAttribute( node, attributeName, NULL );
    bool insertNew = false;
    if( !newAttribute ) {
        newAttribute = NEWATTRIBUTE( node->linearAllocator );
        if( !newAttribute ) return NULL;
        newAttribute->name = CS_linearCopyString( node->linearAllocator, attributeName );
        if( !newAttribute->name ) return NULL;
        insertNew = true;
    }
    struct CS_HtmlValue *newValue = NEWATTRIBUTEVALUE( node->linearAllocator );
    if( !newValue ) {
        return NULL;
    }
    newValue->value = CS_linearCopyString( node->linearAllocator, attributeValue );
    if( !newValue->value ) {
        return NULL;
    }
    newValue->next = newAttribute->values;
    newAttribute->values = newValue;
    if( insertNew ) {
        newAttribute->next = node->attributes;
        node->attributes = newAttribute;
    }
    return newAttribute;
}

static struct CS_HtmlAttribute *privateRemoveAttribute( struct CS_HtmlNode *node, const char *attributeName ) {
    struct CS_HtmlAttribute *prev = NULL;
    struct CS_HtmlAttribute *current = privateFindAttribute( node, attributeName, &prev );
    if( current ) {
        if( prev == NULL ) {
            node->attributes = current->next;
        } else {
            prev->next = current->next;
        }
        current->next = NULL;
    }
    return current;
}

struct CS_HtmlAttribute *CS_htmlGetAttributes(struct CS_HtmlNode *node, const char *attributeName) {
    return privateFindAttribute( node, attributeName, NULL );
}

struct CS_HtmlNode *CS_htmlRemoveAttributeValue(struct CS_HtmlNode *node, const char *attributeName, const char *attributeValue ) {
    if( attributeValue == NULL ) return NULL;
    struct CS_HtmlAttribute *previous = NULL;
    struct CS_HtmlAttribute *attribute = privateFindAttribute( node, attributeName, &previous );
    struct CS_HtmlValue *previousValue= NULL;
    struct CS_HtmlValue *toRemove = attribute->values;
    while( toRemove ) {
        if( strcmp( toRemove->value, attributeValue ) == 0 ) break;
        toRemove = toRemove->next;
    }
    if( toRemove ) {
        if( previousValue ) {
            previousValue->next = toRemove->next;
        } else {
            attribute->values = toRemove->next;
        }
        if( attribute->values == NULL ) {
            if( previous ) {
                previous->next = attribute->next;
            } else {
                node->attributes = attribute->next;
            }
        }
        return node;
    }
    return NULL;
}

struct CS_HtmlNode *CS_htmlRemoveAttributeIndex(struct CS_HtmlNode *node, const char *attributeName, int index ) {
    if( index < 0 ) return NULL;
    struct CS_HtmlAttribute *previous = NULL;
    struct CS_HtmlAttribute *attribute = privateFindAttribute( node, attributeName, &previous );
    if( !attribute ) return NULL;
    struct CS_HtmlValue *previousValue= NULL;
    struct CS_HtmlValue *toRemove = attribute->values;
    for( int i = 0; i < index && toRemove; ++i ) {
        previousValue = toRemove;
        toRemove = toRemove->next;
    }
    if( toRemove ) {
        if( previousValue ) {
            previousValue->next = toRemove->next;
        } else {
            attribute->values = toRemove->next;
        }
        if( attribute->values == NULL ) {
            if( previous ) {
                previous->next = attribute->next;
            } else {
                node->attributes = attribute->next;
            }
        }
        return node;
    }
    return NULL;
}

struct CS_HtmlNode *CS_htmlCreateRoot(const char *name, int initialAlloc) {
    if( !privateCheckOk( name ) ) {
        CS_LOG_ERROR("Attempt to create an HTML node with an invalid name.");
        return NULL;
    }
    void *linearAllocator = CS_linearInit( initialAlloc );
    if( linearAllocator ) {
        struct CS_HtmlNode *returnValue = NEWNODE(linearAllocator);
        if( !returnValue ) {
            CS_linearFree( linearAllocator );
        } else {
            returnValue->name = CS_linearCopyString(linearAllocator, name);
            returnValue->linearAllocator = linearAllocator;
            return returnValue;
        }
    }
    return NULL;
}

struct CS_HtmlAttribute *CS_htmlAddAttribute(struct CS_HtmlNode *node, const char *attributeName, const char *attributeValue) {
    if( !privateCheckOk(attributeName) ) { 
        CS_LOG_ERROR("Tried to create attribute with invalid name.");
        return NULL;
    }
    if( !node || !node->linearAllocator ) return NULL;
    return privateAddAttribute( node, attributeName, attributeValue );
}

struct CS_HtmlNode *CS_htmlRemoveAttribute(struct CS_HtmlNode *node, const char *attributeName) {
    if( !node || !node->linearAllocator ) return NULL;
    return privateRemoveAttribute( node, attributeName )?node:NULL;
}

struct CS_HtmlNode *CS_htmlAddNext(struct CS_HtmlNode *node, const char *name) {
    if( !privateCheckOk( name ) ) {
        CS_LOG_ERROR("Cannot create html node with bad name.");
        return NULL;
    }
    if( !node || !node->linearAllocator ) return NULL;
    struct CS_HtmlNode *returnValue = NEWNODE(node->linearAllocator);
    if( returnValue ) {
        returnValue->name = CS_linearCopyString( node->linearAllocator, name );
        returnValue->next = node->next;
        returnValue->up = node->up;
        if( returnValue->next ) returnValue->next->last = returnValue;
        returnValue->last = node;
        node->next = returnValue;
        returnValue->linearAllocator = node->linearAllocator;
    }
    return returnValue;
}

struct CS_HtmlNode *CS_htmlAddBefore(struct CS_HtmlNode *node, const char *name) {
    if( !privateCheckOk( name ) ) {
        CS_LOG_ERROR("Cannot craete html node with bad name.");
        return NULL;
    }
    if( !node || !node->linearAllocator ) return NULL;
    struct CS_HtmlNode *returnValue = NEWNODE(node->linearAllocator);
    if( returnValue ) {
        returnValue->name = CS_linearCopyString( node->linearAllocator, name );
        returnValue->next = node;
        returnValue->last = node->last;
        returnValue->up = node->up;
        if( returnValue->last ) returnValue->last->next = returnValue;
        if( node->up && node->up->container == node ) node->up->container = returnValue;
        node->last = returnValue;
        returnValue->linearAllocator = node->linearAllocator;
    }
    return returnValue;
}

struct CS_HtmlNode *CS_htmlAddContainerBefore(struct CS_HtmlNode *node, const char *name) {
    if( !privateCheckOk( name ) ) {
        CS_LOG_ERROR("Cannot craete html node with bad name.");
        return NULL;
    }
    if( !node || !node->linearAllocator ) return NULL;
    if( !node->container ) {
        struct CS_HtmlNode *returnValue = NEWNODE(node->linearAllocator);
        if( returnValue ) {
            returnValue->name = CS_linearCopyString( node->linearAllocator, name );
            node->container = returnValue;
            returnValue->up = node;
            returnValue->linearAllocator = node->linearAllocator;
        }
        return returnValue;
    } else {
        return CS_htmlAddBefore( node->container, name );
    }
}

struct CS_HtmlNode *CS_htmlAddContainerAfter(struct CS_HtmlNode *node, const char *name) {
    if( !privateCheckOk( name ) ) {
        CS_LOG_ERROR("Cannot craete html node with bad name.");
        return NULL;
    }
    if( !node || !node->linearAllocator ) return NULL;
    if( !node->container ) {
        struct CS_HtmlNode *returnValue = NEWNODE(node->linearAllocator);
        if( returnValue ) {
            returnValue->name = CS_linearCopyString( node->linearAllocator, name );
            node->container = returnValue;
            returnValue->up = node;
            returnValue->linearAllocator = node->linearAllocator;
        }
        return returnValue;
    } else {
        struct CS_HtmlNode *current = node->container;
        while( current->next ) current = current->next;
        return CS_htmlAddNext( current, name );
    }
}

struct CS_HtmlNode *CS_htmlSetContents(struct CS_HtmlNode *node, const char *contents) {
    if( !node || !node->linearAllocator ) return NULL;
    node->contents = CS_linearCopyString( node->linearAllocator, contents );
    return node->contents?node:NULL;
}

void CS_htmlFree(struct CS_HtmlNode *node) {
    if( !node || !node->linearAllocator ) return;
    CS_linearFree( node->linearAllocator );
}

struct lengthAndValue {
    int length;
    char *value;
};

static struct lengthAndValue quoteLookup[] = {
    //NUL          SOH          STX          ETX          EOT          ENQ          ACK          BEL
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    // BS          TAB           LF           VT           FF           CR           SO           SI
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    //DLE          DC1          DC2          DC3          DC4          NAK          SYN          ETB
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    //CAN           EM          SUB          ESC           FS           GS           RS           US
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    //' '            !            "            #            $            %            &            '
    { 1, NULL }, { 1, NULL }, {6,"&quot;"},{ 0, NULL }, { 0, NULL }, { 0, NULL }, {5,"&amp;"}, { 1, NULL },
    //  (            )            *            +            ,            -            .            /
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  0            1            2            3            4            5            6            7
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  8            9            :            ;            <            =            >            ?
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 4,"&lt;"}, { 1, NULL }, { 4,"&gt;"}, { 1, NULL },
    //  @            A            B            C            D            E            F            G
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  H            I            J            K            L            M            N            O
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  P            Q            R            S            T            U            V            W
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  X            Y            Z            [            \            /            ^            _
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  `            a            b            c            d            e            f            g
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  h            i            j            k            l            m            n            o
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  p            q            r            s            t            u            v            w
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL },
    //  x            y            z            {            |            }            ~          DEL
    { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 1, NULL }, { 0, NULL },
};

static char *privateHtmlQuoted( const char * in ) {
    if( !in ) return NULL;
    int nLen = 0;
    const char *current = in;
    while( *current ) {
        if( *current < 0 ) {
            ++nLen;
        } else {
            nLen += quoteLookup[ (int)*current ].length;
        }
        ++current;
    }
    char *quoted = CS_tempBuff( nLen + 1 );
    char *out = quoted;
    current = in;
    while( *current ) {
        if( *current < 0 ) {
            *out = *current;
            ++out;
        } else {
            struct lengthAndValue *lv = quoteLookup + *current;
            if( lv->length ) {
                if( lv->value ) {
                    memcpy( out, lv->value, lv->length );
                } else {
                    *out = *current;
                }
                out += lv->length;
            }
        }
        ++current;
    }
    *out = 0;
    return quoted;
}

static char *privateStripQuotes(const char *in) {
    int nLen = strlen(in);
    char *returnValue = CS_tempBuff(nLen + 1);
    if( returnValue ) {
        for( int i = 0; i < nLen; ++i ) returnValue[i]=in[i]=='"'?'\'':in[i];
        returnValue[nLen] = 0;
    }
    return returnValue;
}

static struct CS_HtmlNode *privatePrintNodeIntro( struct CS_HtmlNode *node, struct CS_StringBuilder *sb ) {
    CS_SB_printf( sb, "<%s", node->name );
    struct CS_HtmlAttribute *current = node->attributes;
    while( current ) {
        CS_SB_printf( sb, " %s=\"", current->name );
        struct CS_HtmlValue *currentValue = current->values;
        while( currentValue ) {
            CS_SB_append( sb, privateStripQuotes(currentValue->value) );
            if( currentValue->next ) CS_SB_appendChar( sb, ' ' );
            currentValue = currentValue->next;
        }
        CS_SB_appendChar( sb, '"' );
        current = current->next;
    }
    CS_SB_appendChar( sb, '>' );
    return node;
}

static struct CS_HtmlNode *privatePrintNodeMid( struct CS_HtmlNode *node, struct CS_StringBuilder *sb ) {
    char *quoted = privateHtmlQuoted( node->contents );
    if( quoted ) CS_SB_append( sb, quoted );
    return node;
}

static struct CS_HtmlNode *privatePrintNodeOutro( struct CS_HtmlNode *node, struct CS_StringBuilder *sb ) {
    CS_SB_printf( sb, "</%s>", node->name );
    return node;
}

struct CS_StringBuilder *CS_htmlAppend(struct CS_HtmlNode *node, struct CS_StringBuilder *sb ) {
    struct CS_HtmlNode *current = node;
    while( current ) {
        privatePrintNodeIntro( current, sb );
        if( current->container ) {
            current = current->container;
            continue;
        }
        privatePrintNodeMid( current, sb );
        privatePrintNodeOutro( current, sb );
        if( current->next ) {
            current = current->next;
            continue;
        }
        while( current && current->up ) {
            current = current->up;
            privatePrintNodeMid( current, sb );
            privatePrintNodeOutro( current, sb );
            if( current->next ) {
                current = current->next;
                break;
            }
        }
        if( current->up == NULL ) break;
    }
    return sb;
}

struct CS_StringBuilder *CS_htmlToStringBuilder(struct CS_HtmlNode *node, int initialSize) {
    if( !node ) return NULL;

    struct CS_StringBuilder *sb = CS_SB_create(initialSize); 

    struct CS_StringBuilder *returnValue = CS_htmlAppend( node, sb );

    if( returnValue == NULL ) CS_SB_free( sb );

    return returnValue;
}

