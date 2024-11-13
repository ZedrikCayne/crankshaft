#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
//#include <values.h>
#include <math.h>

#include "crankshaftjson.h"
#include "crankshafttempbuff.h"
#include "crankshaftlinearalloc.h"
#include "crankshaftalloc.h"
#include "crankshaftstringbuilder.h"
#include "crankshaftstack.h"
#include "crankshaftstringbuilder.h"
#include "crankshaftlogger.h"

#pragma GCC diagnostic ignored "-Wunused-function"

#define JSON_NODE_ALIGNMENT 8

#define BS (char)8
#define LF (char)10
#define CR (char)13
#define FF (char)14
#define SP ' '
#define HT (char)9
#define START_ARRAY '['
#define END_ARRAY ']'
#define START_OBJECT '{'
#define END_OBJECT '}'
#define QUOTE '"'
#define QUOTE_CHAR '\\'
#define COMMA ','
#define COLON ':'

struct _hexToInt {
    char hex;
    int hexInt;
};

static char hexdigit[]  = "0123456789abcdef0123456789abcdef";
static char hexdigit1[] = "00000000000000001111111111111111";

static int intToUTF8( int codePoint, char *out, int length ) {
    if( length <= 0 || codePoint < 0 ) return 0;                              // 0 = 0000
    if( codePoint <= 0x00007F ) {                                             // 1 = 0001
        if( length < 1 ) return -1;                                           // 2 = 0010
        *out = (char)codePoint;                                               // 3 = 0011
        return 1;                                                             // 4 = 0100
    }                                                                         // 5 = 0101
    if( codePoint <= 0x0007FF ) {                                             // 6 = 0110
        if( length < 2 ) return -1;                                           // 7 = 0111
        *(out + 0) = (char)( 0x0000C0 | ( (0x00003F & codePoint) >>  6 ));    // 8 = 1000
        *(out + 1) = (char)( 0x000080 | ( (0x0007C0 & codePoint) >>  0 ));    // 9 = 1001
        return 2;                                                             // A = 1010
    }                                                                         // B = 1011
    if( codePoint <= 0x00FFFF ) {                                             // C = 1100
        if( length < 3 ) return -1;                                           // D = 1101
        *(out + 0) = (char)( 0x0000E0 | ( (0x00F000 & codePoint) >> 12 ));    // E = 1110
        *(out + 1) = (char)( 0x000080 | ( (0x000FC0 & codePoint) >>  6 ));    // F = 1111
        *(out + 2) = (char)( 0x000080 | ( (0x00003F & codePoint) >>  0 ));    
        return 3;
    }
    if( codePoint <= 0x1FFFFF ) {
        if( length < 4 ) return -1;
        *(out + 0) = (char)( 0x0000F0 | ( (0x1C0000 & codePoint) >> 18 ));
        *(out + 1) = (char)( 0x000080 | ( (0x03F000 & codePoint) >> 12 ));
        *(out + 2) = (char)( 0x000080 | ( (0x000FC0 & codePoint) >>  6 ));
        *(out + 3) = (char)( 0x000080 | ( (0x00003F & codePoint) >>  0 ));
        return 4;
    }
    return -1;
}

static int hexDigitToInt( const char *u ) {
    if( *u < '0' ) return -1;
    if( *u > 'f' ) return -1;
    if( *u <= '9' ) return (int)( *u - '0' );
    if( *u > 'a' ) return (int)( *u - 'a' ) + 10;
    if( *u > 'F' ) return -1;
    if( *u >= 'A' ) return (int)( *u - 'A' ) + 10;
    return -1;
}

static int u4ToUTF8( const char *u4, char *out, int length ) {
    int u8Char = 0;
    int i;
    i = hexDigitToInt( (u4 + 0 ) );
    if( i < 0 ) return -1;
    u8Char = i;
    i = hexDigitToInt( (u4 + 1 ) );
    if( i < 0 ) return -1;
    u8Char = ( u8Char << 4 ) + i;
    i = hexDigitToInt( (u4 + 2 ) );
    if( i < 0 ) return -1;
    u8Char = ( u8Char << 4 ) + i;
    i = hexDigitToInt( (u4 + 3 ) );
    if( i < 0 ) return -1;
    u8Char = ( u8Char << 4 ) + i;
    return intToUTF8( u8Char, out, length );
}

static struct CS_StringBuilder *privateQuoteString(const char *inputString, int len, struct CS_StringBuilder *out) {
    struct CS_StringBuilder *returnValue = out;
    const char *sourceBuff = inputString;
    for( int i = 0; i < len && *sourceBuff != 0; ++i ) {
        if( *sourceBuff < ' ' ) {
            CS_SB_appendChar( returnValue, '\\' );
            switch( *sourceBuff ) {
                case BS:
                    if( CS_SB_appendChar( returnValue, 'b' ) == NULL ) return NULL;
                    break;
                case HT:
                    if( CS_SB_appendChar( returnValue, 't' ) == NULL ) return NULL;
                    break;
                case LF:
                    if( CS_SB_appendChar( returnValue, 'n' ) == NULL ) return NULL;
                    break;
                case CR:
                    if( CS_SB_appendChar( returnValue, 'r' ) == NULL ) return NULL;
                    break;
                case FF:
                    if( CS_SB_appendChar( returnValue, 'f' ) == NULL ) return NULL;
                    break;
                default:
                    if( CS_SB_append( returnValue, "u00" ) == NULL ) return NULL;
                    if( CS_SB_appendChar( returnValue, hexdigit1[ (int)*sourceBuff ] ) == NULL ) return NULL;
                    if( CS_SB_appendChar( returnValue, hexdigit[ (int)*sourceBuff ] ) == NULL ) return NULL;
                    break;
            }
        } else {
            switch( *sourceBuff ) {
                case QUOTE_CHAR:
                case QUOTE:
                    if( CS_SB_appendChar( returnValue, QUOTE_CHAR ) == NULL ) return NULL;
                default:
                    if( CS_SB_appendChar( returnValue, *sourceBuff ) == NULL ) return NULL;
            }
        }
        ++sourceBuff;
    }

    return returnValue;
}

struct CS_StringBuilder *CS_quoteStringToStringBuilder(const char *inputString, int len, struct CS_StringBuilder *out) {
    return privateQuoteString(inputString,len,out);
}

struct CS_StringBuilder *CS_quoteString(const char *inputString, int len) {
    struct CS_StringBuilder *returnValue = CS_SB_create( len * 2 );
    if( returnValue == NULL ) {
        return NULL;
    }
    if( privateQuoteString(inputString, len, returnValue) == NULL ) {
        CS_SB_free( returnValue );
        return NULL;
    }
    return returnValue;
}

#define jsonNode(VA) ((struct CS_JsonNode *)CS_takeLinear(VA,sizeof(struct CS_JsonNode),JSON_NODE_ALIGNMENT))
static struct CS_JsonNode *privateNewNode( struct CS_JsonNode *aNode ) {
    struct CS_JsonNode *returnValue = jsonNode( aNode->voidLinearAllocator );
    returnValue->name = NULL;
    returnValue->next = NULL;
    returnValue->last = NULL;
    returnValue->up = NULL;
    returnValue->typeEnum = CS_JSON_UNKNOWN;
    returnValue->nItemsOrLength = 0;
    returnValue->container = NULL;
    returnValue->voidLinearAllocator = aNode->voidLinearAllocator;
    returnValue->alloc = NULL;
    return returnValue;
}

static struct CS_JsonNode *privateNewNext( struct CS_JsonNode *aNode ) {
    struct CS_JsonNode *returnValue = privateNewNode( aNode );
    if( returnValue ) {
        if( aNode->up ) aNode->up->nItemsOrLength++;
        returnValue->up = aNode->up;
        returnValue->last = aNode;
        returnValue->next = aNode->next;
        aNode->next = returnValue;
    }
    return returnValue;
}

static struct CS_JsonNode *privateNewContained( struct CS_JsonNode *aNode ) {
    struct CS_JsonNode *returnValue = privateNewNode( aNode );
    if( returnValue ) {
        returnValue->up = aNode;
        aNode->container = returnValue;
    }
    return returnValue;
}

static struct CS_JsonNode *privateClose( struct CS_JsonNode *aNode ) {
    //Can't close the root node...
    if( aNode->up == NULL ) return NULL;
    //Cut off the 'last' thing that was pointing to us as 'next' since we won't
    //be there.
    if( aNode->last != NULL ) {
        aNode->last->next = NULL;
    } else {
        aNode->up->container = NULL;
    }
    struct CS_JsonNode *current = NULL;
    current = aNode->up;
    current->next = aNode;
    aNode->up = current->up;
    aNode->last = current;
    return aNode;
}

static struct CS_JsonNode *privateInitJson( int stackSize ) {
    void *linearAlloc = CS_allocLinearAllocator( stackSize );
    if( linearAlloc == NULL ) return NULL;
    struct CS_JsonNode *root = jsonNode(linearAlloc);
    root->name = NULL;
    root->up = NULL;
    root->next = NULL;
    root->typeEnum = CS_JSON_UNKNOWN;
    root->nItemsOrLength = 0;
    root->container = NULL;
    root->voidLinearAllocator = linearAlloc;
    root->alloc = NULL;
    
    return root;
}

void CS_freeJson( struct CS_JsonNode *any ) {
    struct CS_JsonNode *current = any;
    while( current->up != NULL ) current = current->up;
    
    while( current != NULL ) {
        if( current->alloc != NULL ) CS_free( current->alloc );
        current->alloc = NULL;
        //If we are a container, dip into the container. We'll be working our way
        //back up here so let's cut off the container on our way down. 
        if( current->typeEnum <= CS_JSON_OBJECT && current->container != NULL ) {
            struct CS_JsonNode *last = current;
            current = current->container;
            last->container = NULL;
            continue;
        }
        if( current->next ) {
            current = current->next;
            continue;
        }
        if( current->up ) {
            current = current->up;
            continue;
        }
        if( current->up == NULL ) {
            break;
        }
    }
    void *linearAlloc = current->voidLinearAllocator;
    current->voidLinearAllocator = NULL;
    if( linearAlloc != NULL ) CS_freeLinearAllocator( linearAlloc );
}

enum JSON_TOKEN_TYPES {
    JSON_TOKEN_START_OBJECT,
    JSON_TOKEN_END_OBJECT,
    JSON_TOKEN_START_ARRAY,
    JSON_TOKEN_END_ARRAY,
    JSON_TOKEN_QUOTED_STRING,
    JSON_TOKEN_MEMBER_SEPARATOR,
    JSON_TOKEN_KEY_SEPARATOR,
    JSON_TOKEN_INTEGER,
    JSON_TOKEN_FLOAT,
    JSON_TOKEN_false,
    JSON_TOKEN_true,
    JSON_TOKEN_null,
    JSON_TOKEN_EOF,
    JSON_TOKEN_ERROR
};

static int privateJsonTokenToEnumType( int jsonTokenEnum ) {
    switch( jsonTokenEnum ) {
        case JSON_TOKEN_START_OBJECT:
            return CS_JSON_OBJECT;
        case JSON_TOKEN_START_ARRAY:
            return CS_JSON_ARRAY;
        case JSON_TOKEN_QUOTED_STRING:
            return CS_JSON_STRING_QUOTED;
        case JSON_TOKEN_INTEGER:
            return CS_JSON_INTEGER_AS_STRING;
        case JSON_TOKEN_FLOAT:
            return CS_JSON_FLOAT_AS_STRING;
        case JSON_TOKEN_false:
            return CS_JSON_false;
        case JSON_TOKEN_true:
            return CS_JSON_true;
        case JSON_TOKEN_null:
            return CS_JSON_null;
        default:
            return CS_JSON_UNKNOWN;
    }
}

//start and end are inclusive except for strings
//which returns the start and end quote mark
struct JsonToken {
    const char *start;
    const char *end;
    const char *err;
    int enumType;
};

#define TRACE_TOKEN(__TOK) CS_LOG_TRACE("Found a %s",privateJsonTokenEnumToString(__TOK))
#define ONE_CHAR_RETURN(TOK) out->start=out->end=current;out->enumType=TOK;TRACE_TOKEN(TOK);return TOK

static int privateParseQuotedString( struct JsonToken *out, const char *input, int inputLength ) {
    const char *current = input;
    const char *end = input + inputLength;
    char *temp;
    if( *input != QUOTE )
        return JSON_TOKEN_ERROR;
    out->start = current;
    out->end = NULL;
    out->enumType = JSON_TOKEN_QUOTED_STRING;
    ++current;
    while( current < end && *current ) {
        if( *current == QUOTE_CHAR ) {
            if( current + 2 >= end ) goto END_OF_STRING;
            ++current;
            switch( *current ) {
                case 'b':
                case 't':
                case 'r':
                case 'n':
                case 'f':
                case '"':
                case '/':
                case QUOTE_CHAR:
                    ++current;
                    continue;
                case 'u':
                    for( int i = 0; i < 4; ++i ) {
                        ++current;
                        if( current >= end ) goto END_OF_STRING;
                        switch(*current) {
                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9':
                            case 'a':
                            case 'b':
                            case 'c':
                            case 'd':
                            case 'e':
                            case 'f':
                            case 'A':
                            case 'B':
                            case 'C':
                            case 'D':
                            case 'E':
                            case 'F':
                                break;
                            default:
                                out->err = "Invalid hex digit after \\u";
                                return JSON_TOKEN_ERROR;
                        }
                    }
                    ++current;
                    break;
                default:
                    temp = CS_tempBuff( 1024 );
                    snprintf( temp, 1024, "Invalid quote character %c", *current );
                    out->err = temp;
                    return JSON_TOKEN_ERROR;
            }
            continue;
        }
        if( *current > 0 && *current < SP ) {
            out->err = "Unquoted control character.";
            return JSON_TOKEN_ERROR;
        }
        if( *current == QUOTE ) {
            out->end = current;
            return JSON_TOKEN_QUOTED_STRING;
        }
        ++current;
    }
END_OF_STRING:
    out->err = "Hit end of stream before closing quote.";
    return JSON_TOKEN_ERROR;
}

static int privateParseString( struct JsonToken *out,
                               const char *input,
                               int inputLength,
                               const char *cmpTo,
                               int cmpLength,
                               int returnValue ) {
    if( inputLength < cmpLength ) {
        char *temp = CS_tempBuff( 256 );
        snprintf( temp, 256, "Not enough characters left in buffer to parse %s", cmpTo );
        out->err = temp;
        return JSON_TOKEN_ERROR;
    }
    for( int i = 1; i < cmpLength; ++i ) if( input[i] != cmpTo[i] ) {
        char *temp = CS_tempBuff( 256 );
        snprintf( temp, 256, "Error parsing %s", cmpTo );
        out->err = temp;
        return JSON_TOKEN_ERROR;
    }
    out->start = input;
    out->end = input + cmpLength - 1;
    out->enumType = returnValue;
    return returnValue;
}

enum ParseNumberState {
    PN_OKAY,
    PN_LEADING_0,
    PN_INTEGER,
    PN_FRACTIONAL,
    PN_E_LEADING_0, //Any state bigger than this is invalid for another e.
    PN_E_INTEGER,
    PN_E_FRACTIONAL,
    PN_INVALID,
    PN_INITIAL_SIGN,
    PN_DECIMAL,
    PN_E,
    PN_E_INITIAL_SIGN,
    PN_E_DECIMAL,
};

const char *privateJsonTokenEnumToString(int jsonTokenEnum) {
    switch( jsonTokenEnum ) {
        case JSON_TOKEN_START_OBJECT:
            return "JSON_TOKEN_START_OBJECT";
        case JSON_TOKEN_END_OBJECT:
            return "JSON_TOKEN_END_OBJECT";
        case JSON_TOKEN_START_ARRAY:
            return "JSON_TOKEN_START_ARRAY";
        case JSON_TOKEN_END_ARRAY:
            return "JSON_TOKEN_END_ARRAY";
        case JSON_TOKEN_QUOTED_STRING:
            return "JSON_TOKEN_QUOTED_STRING";
        case JSON_TOKEN_MEMBER_SEPARATOR:
            return "JSON_TOKEN_MEMBER_SEPARATOR";
        case JSON_TOKEN_KEY_SEPARATOR:
            return "JSON_TOKEN_KEY_SEPARATOR";
        case JSON_TOKEN_INTEGER:
            return "JSON_TOKEN_INTEGER";
        case JSON_TOKEN_FLOAT:
            return "JSON_TOKEN_FLOAT";
        case JSON_TOKEN_false:
            return "JSON_TOKEN_false";
        case JSON_TOKEN_true:
            return "JSON_TOKEN_true";
        case JSON_TOKEN_null:
            return "JSON_TOKEN_null";
        case JSON_TOKEN_EOF:
            return "JSON_TOKEN_EOF";
        case JSON_TOKEN_ERROR:
            return "JSON_TOKEN_ERROR";
    }
    return "INVALID ENUM";
}

static const char *numberStateToString( int enumParseNumberState ) {
    switch( enumParseNumberState ) {
    	case PN_OKAY:
            return "PN_OKAY";
        case PN_LEADING_0:
            return "PN_LEADING_0";
    	case PN_INTEGER:
            return "PN_INTEGER";
    	case PN_FRACTIONAL:
            return "PN_FRACTIONAL";
    	case PN_E_LEADING_0: //Any state bigger than this is invalid for another e.
            return "PN_E_LEADING_0";
    	case PN_E_INTEGER:
            return "PN_E_INTEGER";
    	case PN_E_FRACTIONAL:
            return "PN_E_FRACTIONAL";
    	case PN_INVALID:
            return "PN_INVALID";
    	case PN_INITIAL_SIGN:
            return "PN_INITIAL_SIGN";
    	case PN_DECIMAL:
            return "PN_DECIMAL";
    	case PN_E:
            return "PN_E";
    	case PN_E_INITIAL_SIGN:
            return "PN_E_INITIAL_SIGN";
    	case PN_E_DECIMAL:
            return "PN_E_DECIMAL";
    }
    return "PN_INVALID";
}
    

#define RETURN_ERROR(STRING) out->err=STRING;out->end=current;return JSON_TOKEN_ERROR
static int privateParseNumber( struct JsonToken *out, const char *input, int inputLength ) {
    const char *current = input;
    const char *end = input + inputLength;
    out->start = input;
    out->end = NULL;
    out->err = 0;
    int enumValue = JSON_TOKEN_INTEGER;
    int parseState = PN_INTEGER;
    if( *current == '-' )
        parseState = PN_INITIAL_SIGN;
    if( *current == '0' )
        parseState = PN_LEADING_0;
    ++current;
    while( current < end && *current ) {
        CS_LOG_TRACE("*current %c state: %s", *current, numberStateToString( parseState ) );
        switch( *current ) {
            default:
                if( parseState >= PN_INVALID ) {
                    RETURN_ERROR( "Number ended too early." );
                }
                out->end = current - 1;
                out->enumType = enumValue;
                return enumValue;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if( parseState == PN_LEADING_0 || parseState == PN_E_LEADING_0 ) {
                    RETURN_ERROR( "Integer portion started with a 0. Next character must be a . or whitespace" );
                }
                else if( parseState == PN_INITIAL_SIGN ) {
                    if( *current == '0' ) {
                        parseState = PN_LEADING_0;
                    } else {
                        parseState = PN_INTEGER;
                    }
                } else if( parseState == PN_DECIMAL ) {
                    parseState = PN_FRACTIONAL;
                } else if( parseState == PN_E || parseState == PN_E_INITIAL_SIGN ) {
                    parseState = PN_E_INTEGER;
                } else if( parseState == PN_E_DECIMAL ) {
                    parseState = PN_E_FRACTIONAL;
                }
                ++current;
                continue;
            case '.':
                if( parseState == PN_INITIAL_SIGN ) {
                    RETURN_ERROR( "Number started with a - but followed up by a ." );
                } else if( parseState == PN_E || parseState == PN_E_INITIAL_SIGN ) {
                    RETURN_ERROR( "Should be a number after e not a ." );
                } else if( parseState == PN_DECIMAL || parseState == PN_E_DECIMAL ) {
                    RETURN_ERROR( "Number has two . next to each other.");
                } else if( parseState == PN_E_FRACTIONAL || parseState == PN_FRACTIONAL ) {
                    RETURN_ERROR( "Number has more than one . in it." );
                } else if( parseState == PN_INTEGER || parseState == PN_LEADING_0 ) {
                    parseState = PN_DECIMAL;
                    enumValue = JSON_TOKEN_FLOAT;
                } else if( parseState == PN_E_INTEGER || parseState == PN_E_LEADING_0 ) {
                    parseState = PN_E_DECIMAL;
                } else {
                    RETURN_ERROR("Not sure how we got here. we should  have handled everything else.");
                }
                ++current;
                continue;
            case 'e':
            case 'E':
                if( parseState >= PN_E_LEADING_0 ) {
                    RETURN_ERROR("Number has a poorly placed e.");
                }
                parseState = PN_E;
                enumValue = JSON_TOKEN_FLOAT;
                ++current;
                continue;
            case '+':
            case '-':
                if( parseState != PN_E ) {
                    RETURN_ERROR("Number has a sign in it that is not the initial or after the e");
                }
                parseState = PN_E_INITIAL_SIGN;
                ++current;
                continue;
        }
    }
    if( parseState > PN_INVALID )
        return JSON_TOKEN_ERROR;
    out->end = current - 1;
    out->enumType = enumValue;
    return enumValue;
}

#define PRINTABLE_CHAR_LENGTH 32
static char *printableStringOfThisChar(char *thisChar) {
    char * returnValue = CS_tempBuff(PRINTABLE_CHAR_LENGTH);
    if( *thisChar < 32 || *thisChar > 127 ) {
        snprintf( returnValue, PRINTABLE_CHAR_LENGTH, "0x%02X", (int)(*thisChar) );
    } else {
        snprintf( returnValue, PRINTABLE_CHAR_LENGTH, "'%c'", *thisChar );
    }
    return returnValue;
}

static int privateParseNext( struct JsonToken *out, const char *input, int inputLength) {
    const char *current = input;
    const char *end = input + inputLength;
    int foundEnum = 0;
    while( current < end ) {
        switch( *current ) {
            case 0:
                RETURN_ERROR("Null in json.");
            case HT:
            case SP:
            case CR:
            case LF:
                ++current;
                continue;
            case START_OBJECT:
                ONE_CHAR_RETURN(JSON_TOKEN_START_OBJECT);
            case END_OBJECT:
                ONE_CHAR_RETURN(JSON_TOKEN_END_OBJECT);
            case START_ARRAY:
                ONE_CHAR_RETURN(JSON_TOKEN_START_ARRAY);
            case END_ARRAY:
                ONE_CHAR_RETURN(JSON_TOKEN_END_ARRAY);
            case QUOTE:
                foundEnum = privateParseQuotedString( out, current, inputLength - (current - input) );
                TRACE_TOKEN(foundEnum);
                return foundEnum;
            case COMMA:
                ONE_CHAR_RETURN( JSON_TOKEN_MEMBER_SEPARATOR );
            case COLON:
                ONE_CHAR_RETURN( JSON_TOKEN_KEY_SEPARATOR );
            case 't':
                foundEnum = privateParseString( out, current, inputLength - (current - input), "true", 4, JSON_TOKEN_true );
                TRACE_TOKEN(foundEnum);
                return foundEnum;
            case 'f':
                foundEnum = privateParseString( out, current, inputLength - (current - input), "false", 5, JSON_TOKEN_false );
                TRACE_TOKEN(foundEnum);
                return foundEnum;
            case 'n':
                foundEnum = privateParseString( out, current, inputLength - (current - input), "null", 4, JSON_TOKEN_null );
                TRACE_TOKEN(foundEnum);
                return foundEnum;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                foundEnum = privateParseNumber( out, current, inputLength - (current - input) );
                TRACE_TOKEN(foundEnum);
                return foundEnum;
                  
            default:
                RETURN_ERROR("Badly formatted json");
        }
    }
    return JSON_TOKEN_EOF;
}

enum PrivateJsonParseState {
    JSON_PARSE_START,
    JSON_PARSING_MAP,
    JSON_PARSING_ARRAY,
    JSON_PARSING_KEY,
    JSON_PARSING_VALUE,
};

static const char *copyOrMangleInPlace( struct JsonToken *token, struct CS_JsonNode *node, bool copy, bool fillStringValue ) {
    char *start = (char*)(token->start);
    char *end = (char*)(token->end);
    if( token->enumType == JSON_TOKEN_QUOTED_STRING ) {
        start++;
    } else {
        end++;
    }
    int len = (end - start);
    char *stringValue = NULL;
    if( copy ) {
        stringValue = CS_takeLinear(node->voidLinearAllocator, len + 1, 1);
        if( stringValue == NULL ) return NULL;
        memcpy( stringValue, start, len );
        if( fillStringValue ) node->stringValue = stringValue;
    } else {
        stringValue = start;
        if( fillStringValue ) node->stringValue = start;
    }
    if( fillStringValue ) node->nItemsOrLength = len;
    stringValue[ len ] = 0;
    return stringValue;
}

static struct CS_JsonNode *pushOn( struct JsonToken *token, const char *name, struct CS_JsonNode *node, bool copy ) {
    struct CS_JsonNode *returnValue = node;
    if( node->typeEnum != CS_JSON_UNKNOWN ) {
        CS_LOG_ERROR("Trying to re-initialize a json node.");
        return NULL;
    }
    if( name != NULL ) {
        //If we have a name, we want to be part of an object..if we aren't bail.
        if( node->up == NULL ||
            node->up->typeEnum != CS_JSON_OBJECT ) {
            CS_LOG_ERROR("Trying to add a value with a name to a non-object");
            return NULL;
        }
    } else {
        if( token->enumType != JSON_TOKEN_END_OBJECT && node->up && node->up->typeEnum == CS_JSON_OBJECT ) {
            CS_LOG_ERROR("Trying to add something to an object that doesn't have a name");
            return NULL;
        }
    }
    node->name = name;
    node->typeEnum = privateJsonTokenToEnumType(token->enumType);
    //This is either a separator or close.
    const char *stringValue;
    switch( token->enumType ) {
        case JSON_TOKEN_START_OBJECT:
        case JSON_TOKEN_START_ARRAY:
            returnValue = privateNewContained( node );
            break;
        case JSON_TOKEN_END_OBJECT:
            if( node->up == NULL ) {
                CS_LOG_ERROR("Trying to close an object but we are at the topmost level of the json.");
                return NULL;
            }
            if( node->up->typeEnum != CS_JSON_OBJECT ) {
                CS_LOG_ERROR("Trying to close an object but we are not inside of an object.");
                return NULL;
            }
            returnValue = privateClose( node );
            break;
        case JSON_TOKEN_END_ARRAY:
            if( node->up == NULL ) {
                CS_LOG_ERROR("Trying to close an array but we are at the topmost level of the json.");
                return NULL;
            }
            if( node->up->typeEnum != CS_JSON_ARRAY ) {
                CS_LOG_ERROR("Trying to close an array but we weren't in the middle of an array.");
                return NULL;
            }
            returnValue = privateClose( node );
            break;
        case JSON_TOKEN_QUOTED_STRING:
        case JSON_TOKEN_INTEGER:
        case JSON_TOKEN_FLOAT:
            stringValue = copyOrMangleInPlace(token,node,copy,true);
            if( stringValue == NULL ) {
                return NULL;
            }
        case JSON_TOKEN_false:
        case JSON_TOKEN_true:
        case JSON_TOKEN_null:
            returnValue = privateNewNext(node);
            break;

        default:
            CS_LOG_ERROR("Unexpected token");
            return NULL;
    }
    return returnValue;
}

struct CS_JsonNode *privateParseJson(const char *input, int inputLength, int allocSize, bool copy ) {
    struct CS_JsonNode *returnValue = privateInitJson( allocSize );
    const char *current;
    const char *end;

    current = input;
    end = input + inputLength;

    struct CS_JsonNode *currentNode = returnValue;
    struct JsonToken out[2] = {0};

    const char *name[2] = {NULL,NULL};

    bool needAComma = false;
    int tokenType = JSON_TOKEN_ERROR;
    int lastToken = JSON_TOKEN_ERROR;
    int nextToken = JSON_TOKEN_ERROR;
    int outToggle = 0;
    nextToken = privateParseNext( out + outToggle, current, inputLength - (current - input) );

    while(current < end) {
        lastToken = tokenType;
        tokenType = nextToken;

        if( tokenType == JSON_TOKEN_EOF )
            break;
        if( tokenType == JSON_TOKEN_ERROR )
            goto ERROR_DEL_JSON;

        if( tokenType != JSON_TOKEN_MEMBER_SEPARATOR ) {
            if( tokenType == JSON_TOKEN_END_ARRAY ||
                tokenType == JSON_TOKEN_END_OBJECT ) {
                if( lastToken == JSON_TOKEN_MEMBER_SEPARATOR ) {
                    out[outToggle].err = "Closing an object or array with a dangling comma.";
                    goto ERROR_DEL_JSON;
                }
            } else {
                if( needAComma ) {
                    out[outToggle].err = "Missing a , or a close object/array.";
                    goto ERROR_DEL_JSON;
                }
                if( currentNode->up != NULL && currentNode->up->typeEnum == CS_JSON_OBJECT ) {
                    //This had better be a quoted string.
                    if( tokenType != JSON_TOKEN_QUOTED_STRING ) {
                        out[outToggle].err = ("Got anything other than a string for the 'key' of a json object");
                        goto ERROR_DEL_JSON;
                    }
                    name[outToggle] = copyOrMangleInPlace(out + outToggle,currentNode,copy,false);
                    //CS_LOG_TRACE("Name %s on toggle %d set", name[outToggle], outToggle);
                    current = out[outToggle].end + 1;
                    tokenType = privateParseNext( out + outToggle, current, inputLength - (current - input) );
                    if( tokenType != JSON_TOKEN_KEY_SEPARATOR ) {
                        out[outToggle].err = ("Need a : between key and value.");
                        goto ERROR_DEL_JSON;
                    }
                    current = out[outToggle].end + 1;
                    tokenType = privateParseNext( out + outToggle, current, inputLength - (current - input) );
                    if( tokenType == JSON_TOKEN_EOF ) {
                        out[outToggle].err = "Ran out of bytes reading a value in an object.";
                        goto ERROR_DEL_JSON;
                    }
                    if( tokenType == JSON_TOKEN_ERROR ) {
                        goto ERROR_DEL_JSON;
                    }
                }
            }
        } else {
            if( !needAComma ) {
                out[outToggle].err = "Unexpected , in json.";
                goto ERROR_DEL_JSON;
            }
        }
  
        current = out[outToggle].end + 1;
        outToggle = 1 - outToggle;
        nextToken = privateParseNext( out + outToggle, current, inputLength - (current - input) );
        if( tokenType != JSON_TOKEN_MEMBER_SEPARATOR ) {
            currentNode = pushOn( out + (1-outToggle), name[1-outToggle], currentNode, copy );
        }
        if( currentNode == NULL ) {
            goto ERROR_DEL_JSON;
        }
        needAComma = (tokenType != JSON_TOKEN_START_OBJECT && tokenType != JSON_TOKEN_START_ARRAY && tokenType != JSON_TOKEN_MEMBER_SEPARATOR) && (currentNode->up != NULL);
        name[1-outToggle] = NULL;
    }

    //Didn't read anything, we wanna bail.
    if( currentNode == returnValue ) {
        CS_LOG_ERROR("We didn't get anything to parse for JSON.");
        goto ERROR_DEL_JSON;
    }
    if( currentNode != returnValue->next ) {
        CS_LOG_ERROR("Unclosed json.");
        goto ERROR_DEL_JSON;
    }

    //There is a dangling current node
    if( currentNode->last ) {
        currentNode->last->next = NULL;
    }
    if( currentNode->up && currentNode->up->container == currentNode ) {
        currentNode->up->container = NULL;
    }

    return returnValue;
ERROR_DEL_JSON:
    if( out[outToggle].err ) {
        CS_LOG_ERROR("%s", out[outToggle].err);
    } else {
        CS_LOG_ERROR("ERROR NOT SET!");
    }
    CS_freeJson( returnValue );
    return NULL;
}

struct CS_JsonNode *CS_parseJsonCopy(const char *input, int inputLength, int allocSize) {
    return privateParseJson(input,inputLength,allocSize,true);
}

struct CS_JsonNode *CS_parseJson(const char *input, int inputLength, int allocSize) {
    return privateParseJson(input,inputLength,allocSize,false);
}

static int unquotePrivate( char *output, char *inputString, int len ) {
    char *outputBuffer = output;
    char *outputBufferEnd = outputBuffer + len;
    const char *sourceBuffer;
    const char *sourceBufferEnd = inputString + len;

    for( int i = 0 ; i < len; ++i ) {
        sourceBuffer = inputString + i;
        if( *sourceBuffer == 0 )
            break;
        if( outputBuffer >= outputBufferEnd )
            return -1;
        if( *sourceBuffer == '\\' ) {
            ++sourceBuffer;
            ++i;
            if( i >= len ) return -1;
            switch( *sourceBuffer ) {
                case QUOTE_CHAR:
                    *outputBuffer = QUOTE_CHAR;
                    ++outputBuffer;
                    break;
                case QUOTE:
                    *outputBuffer = QUOTE;
                    ++outputBuffer;
                    break;
                case 'b':
                    *outputBuffer = BS;
                    ++outputBuffer;
                    break;
                case 't':
                    *outputBuffer = HT;
                    ++outputBuffer;
                    break;
                case 'n':
                    *outputBuffer = LF;
                    ++outputBuffer;
                    break;
                case 'r':
                    *outputBuffer = CR;
                    ++outputBuffer;
                    break;
                case 'f':
                    *outputBuffer = FF;
                    ++outputBuffer;
                    break;
                case 'u':
                    //eat the u
                    ++sourceBuffer;
                    if( sourceBuffer + 4 > sourceBufferEnd ) {
                        CS_LOG_ERROR("Ran out of characters decoding \\u unquoting json");
                        return -1;
                    }
                    int length = u4ToUTF8( sourceBuffer, outputBuffer, outputBufferEnd - outputBuffer );
                    if( length < 0 ) {
                        CS_LOG_ERROR("Output buffer too smaall during unquoting json");
                        return -1;
                    }
                    outputBuffer += length;
                    //eat the u and 3 of the 4 zeroes
                    i+=4;
                    break;
                default:
                    CS_LOG_ERROR("Error unquoting json, bad \\ character '%c'", *sourceBuffer);
                    return true;
            }
        } else {
            *outputBuffer = *sourceBuffer;
            ++outputBuffer;
        }
        ++sourceBuffer;
    }
    if( outputBuffer > outputBufferEnd ) {
        CS_LOG_ERROR("Error unquoting json, output buffer too small");
        return -1;
    }
    *outputBuffer = 0;
    return outputBuffer - output;
}

bool CS_unquoteInPlace(char *inputString, int len) {
    return unquotePrivate(inputString, inputString, len) >= 0;
}

struct CS_StringBuilder *CS_unquoteString(const char *inputString, int len) {
    //No matter what, the output string will be either <= the length of the input string
    struct CS_StringBuilder *returnValue = CS_SB_create(len + 2);
    int newLength = unquotePrivate( returnValue->buffer, (char*)inputString, len );
    if( newLength < 0 ) {
        CS_SB_free( returnValue );
        return NULL;
    }
    returnValue->currentHead = newLength;
    return returnValue;
}
 
const char *CS_jsonEnumTypeAsString(const int enumType) {
    switch ( enumType ) {
        case CS_JSON_ARRAY:
            return "CS_JSON_ARRAY";
        case CS_JSON_OBJECT:
            return "CS_JSON_OBJECT";
        case CS_JSON_STRING_QUOTED:
            return "CS_JSON_STRING_QUOTED";
        case CS_JSON_STRING_UNQUOTED:
            return "CS_JSON_STRING_UNQUOTED";
        case CS_JSON_STRING_QUOTED_ALLOCATED:
            return "CS_JSON_STRING_QUOTED_ALLOCATED";
        case CS_JSON_STRING_UNQUOTED_ALLOCATED:
            return "CS_JSON_STRING_UNQUOTED_ALLOCATED";
        case CS_JSON_INTEGER:
            return "CS_JSON_INTEGER";
        case CS_JSON_INTEGER_AS_STRING:
            return "CS_JSON_INTEGER_AS_STRING";
        case CS_JSON_FLOAT:
            return "CS_JSON_FLOAT";
        case CS_JSON_FLOAT_AS_STRING:
            return "CS_JSON_FLOAT_AS_STRING";
        case CS_JSON_true:
            return "CS_JSON_true";
        case CS_JSON_false:
            return "CS_JSON_false";
        case CS_JSON_null:
            return "CS_JSON_null";
        case CS_JSON_UNKNOWN:
            return "CS_JSON_UNKNOWN";
    }
    return "CS_JSON_ERROR";
}

#define STACK_DEPTH 64

static bool appendQuoted( struct CS_StringBuilder *sb, const char *string ) {
    return CS_SB_append(sb,"\"") != NULL &&
            CS_SB_append(sb,string) != NULL &&
            CS_SB_append(sb,"\"") != NULL;
}

bool CS_jsonNodesEquivalent(struct CS_JsonNode *a, struct CS_JsonNode *b) {
    if( (a == NULL) ^ (b == NULL) )
        return false;
    if( a == NULL )
        return true;
    const struct CS_JsonNode *currenta = a;
    const struct CS_JsonNode *currentb = b;
    while( (currenta != NULL) &&
           (currentb != NULL) ) {
        if( currenta->typeEnum != currentb->typeEnum ) {
            CS_LOG_ERROR("Types don't match %s vs %s.",CS_jsonEnumTypeAsString(currenta->typeEnum), CS_jsonEnumTypeAsString(currentb->typeEnum));
            return false;
        }
        if( (currenta->name != NULL) ^ (currentb->name != NULL) ) {
            CS_LOG_ERROR("Names aren't both null or occupied.");
            return false;
        }
        if( currenta->name ) {
            if( strcmp( currenta->name, currentb->name ) != 0 ) {
                CS_LOG_ERROR("Names don't match.");
                return false;
            }
        }
        switch( currenta->typeEnum ) {
            case CS_JSON_ARRAY:
            case CS_JSON_OBJECT:
                if( (currenta->container != NULL) ^ (currentb->container != NULL) ) {
                    CS_LOG_ERROR("One container is empthy, the other is not.");
                    return false;
                }
                if( currenta->container ) {
                    currenta = currenta->container;
                    currentb = currentb->container;
                    continue;
                }
                break;
            case CS_JSON_STRING_QUOTED:
            case CS_JSON_STRING_UNQUOTED:
            case CS_JSON_INTEGER_AS_STRING:
            case CS_JSON_FLOAT_AS_STRING:
                if( strcmp( currenta->stringValue, currentb->stringValue ) != 0 ){
                    CS_LOG_ERROR( "Strings don't match. '%s' vs '%s'", currenta->stringValue, currentb->stringValue );
                    return false;
                }
                break;
            case CS_JSON_STRING_QUOTED_ALLOCATED:
            case CS_JSON_STRING_UNQUOTED_ALLOCATED:
                if( strcmp( currenta->alloc, currentb->alloc ) != 0 ) {
                    CS_LOG_ERROR( "Allocated strings don't match. '%s' vs '%s'", (char*)currenta->alloc, (char*)currentb->alloc );
                    return false;
                }
                break;
            case CS_JSON_INTEGER:
                if( currenta->intValue != currentb->intValue ) {
                    CS_LOG_ERROR("Integer values don't match.");
                    return false;
                }
                break;
            case CS_JSON_FLOAT:
                if( currenta->floatValue != currentb->floatValue ) {
                    CS_LOG_ERROR("Float values don't match.");
                    return false;
                }
                break;
            case CS_JSON_true:
                break;
            case CS_JSON_false:
                break;
            case CS_JSON_null:
                break;
        }
        
        while( currenta && (currenta->next == NULL) ) {
            if( (currentb == NULL) || (currentb->next != NULL) ) {
                CS_LOG_ERROR("Structure doesn't match. (Nexts don't match)");
                return false;
            }
            currenta = currenta->up;
            currentb = currentb->up;
        }
        if( currenta ) {
            if( !currentb ) {
                CS_LOG_ERROR("Structure doesn't match. (ups don't match)");
                return false;
            }
            currenta = currenta->next;
            currentb = currentb->next;
        }
    }
    return true;
}

static bool appendUnquoted( struct CS_StringBuilder *sb, const char *string ) {
    CS_SB_appendChar(sb,'"');
    bool returnValue = privateQuoteString( string, strlen(string), sb) == NULL;
    CS_SB_appendChar(sb,'"');
    return returnValue;
}

struct CS_StringBuilder *CS_jsonNodePrintable(const struct CS_JsonNode *printMe) {
    struct CS_StringBuilder *sb = CS_SB_create(2048);

    if( printMe == NULL ) CS_SB_append(sb, "NULL");

    const struct CS_JsonNode *current = printMe;

    while( current && current->typeEnum < CS_JSON_UNKNOWN ) {
        if( current->name ) {
            appendQuoted(sb,current->name);
            CS_SB_append(sb, ":");
        }
        switch( current->typeEnum ) {
            case CS_JSON_ARRAY:
                if( current->container ) {
                    CS_SB_append(sb, "[");
                    current = current->container;
                    continue;
                } else {
                    CS_SB_append(sb, "[]");
                }
                
                break;
            case CS_JSON_OBJECT:
                if( current->container ) {
                    CS_SB_append(sb, "{");
                    current = current->container;
                    continue;
                } else {
                    CS_SB_append(sb, "{}");
                }
                break;
            case CS_JSON_STRING_QUOTED:
                appendQuoted(sb, current->stringValue );
                break;
            case CS_JSON_STRING_UNQUOTED:
                appendUnquoted(sb, current->stringValue );
                break;
            case CS_JSON_STRING_QUOTED_ALLOCATED:
                appendQuoted(sb, (char*)current->alloc );
                break;
            case CS_JSON_STRING_UNQUOTED_ALLOCATED:
                appendUnquoted(sb, (char*)current->alloc );
                break;
            case CS_JSON_INTEGER:
                CS_SB_printf(sb, "%lld", current->intValue );
                break;
            case CS_JSON_INTEGER_AS_STRING:
            case CS_JSON_FLOAT_AS_STRING:
                CS_SB_append(sb, current->stringValue );
                break;
            case CS_JSON_FLOAT:
                CS_SB_printf(sb, "%f", current->floatValue );
                break;
            case CS_JSON_true:
                appendQuoted(sb,"true");
                break;
            case CS_JSON_false:
                appendQuoted(sb,"false");
                break;
            case CS_JSON_null:
                appendQuoted(sb,"null");
                break;
        }
        while( current && current->next == NULL ) {
            current = current->up;
            if( current ) {
                if( current->typeEnum == CS_JSON_ARRAY ) {
                    CS_SB_appendChar(sb, ']');
                } else {
                    CS_SB_appendChar(sb, '}');
                }
            }
        }
        if( current && current->next ) {
            CS_SB_appendChar(sb,',');
        }
        if( current ) current = current->next;
    }
    return sb;
}

static char *privateAllocStringWithLength( struct CS_JsonNode *from, const char *nullTermString, int len ) {
    char *buff = CS_takeLinear( from->voidLinearAllocator, len + 1, 1 );
    if( buff ) {
        strncpy( buff, nullTermString, len + 1 );
    }
    return buff;
}

static char *privateAllocString( struct CS_JsonNode *from, const char *nullTermString ) {
    int len = strlen( nullTermString );
    return privateAllocStringWithLength( from, nullTermString, len );
}

static struct CS_JsonNode *privateAppend( struct CS_JsonNode *appendTo, const char *name ) {
    char *setName = NULL;
    if( name == NULL ) {
        if( appendTo->up && appendTo->up->typeEnum == CS_JSON_OBJECT ) {
            CS_LOG_ERROR("Trying to append a node without a name to an object.");
            return NULL;
        }
        if( appendTo->up == NULL && appendTo->typeEnum != CS_JSON_UNKNOWN ) {
            CS_LOG_ERROR("Trying to append to a root node.");
            return NULL;
        }
    } else {
        if( appendTo->up == NULL || appendTo->up->typeEnum != CS_JSON_OBJECT ) {
            CS_LOG_ERROR("Trying to append a node with a key to a non-object");
            return NULL;
        }
        setName = privateAllocString( appendTo, name );
        if( setName == NULL ) {
            CS_LOG_ERROR("Key of node too long for the allocator.");
            return NULL;
        }
    }
    if( appendTo->typeEnum == CS_JSON_UNKNOWN ) return appendTo;
    struct CS_JsonNode *returnValue = privateNewNext( appendTo );
    returnValue->name = setName;
    return returnValue;
}

static struct CS_JsonNode *privateAdd( struct CS_JsonNode *addTo, const char *name ) {
    const char *setName = NULL;
    if( name == NULL ) {
        if( addTo->typeEnum != CS_JSON_ARRAY ) {
            CS_LOG_ERROR("Trying to add a value without a key to a non-array.");
            return NULL;
        }
    } else {
        if( addTo->typeEnum != CS_JSON_OBJECT ) {
            CS_LOG_ERROR("Trying to add a value with a key to a non object.");
            return NULL;
        }
        setName = privateAllocString( addTo, name );
        if( setName == NULL ) {
            CS_LOG_ERROR("Key of node too long for the allocator.");
            return NULL;
        }
    }

    struct CS_JsonNode *returnValue = NULL;
    if( addTo->container ) {
        struct CS_JsonNode *current = addTo->container;
        while( current->next != NULL ) current = current->next;
        returnValue = privateNewNext( current );
    } else {
        returnValue = privateNewContained( addTo );
    }
    returnValue->name = setName;

    return returnValue;
}

struct CS_JsonNode *CS_jsonNodeRemoveNode( struct CS_JsonNode *remove ) {
    //Can't nuke the top most node.
    if( remove->up == NULL ) return NULL;
    
    //Are we removing the first thing in the container?
    if( remove->up->container == remove ) {
        remove->up->container = remove->next;
    }
    //Change the last/next nodes (if they exist)
    if( remove->next != NULL ) remove->next->last = remove->last;
    if( remove->last != NULL ) remove->last->next = remove->next;
    remove->up->nItemsOrLength--;
    if( remove->alloc ) CS_free( remove->alloc );
    remove->alloc = NULL;

    struct CS_JsonNode *oldUp = remove->up;

    //In case this is some form of container, we need to clean up any
    //possible external allocations under this. Chop off the underside
    //and null out the allocator and go to town.
    if( remove->typeEnum == CS_JSON_OBJECT ||
        remove->typeEnum == CS_JSON_ARRAY ) {
        remove->up = NULL;
        remove->voidLinearAllocator = NULL;
        CS_freeJson( remove );
    }

    //We're either going to return the thing which would have replaced
    //this node (the next) the one before it if it was the last one
    //(the last) or if we've emptied the container out, the
    //container (up)
    if( remove->next ) return remove->next;
    if( remove->last ) return remove->last;

    return oldUp;
}

static struct CS_JsonNode *stuffQuotedSBOntoJsonNodeAndFreeSB( struct CS_JsonNode *onto, struct CS_StringBuilder *quoted ) {
    struct CS_JsonNode *returnValue = onto;
    const char *stackAllocatedString = privateAllocStringWithLength(returnValue,quoted->buffer,CS_SB_length(quoted));
    //String might be too long to fit into the linear allocator of the json node.
    //If it is, just stuff it into the alloc.
    returnValue->nItemsOrLength = CS_SB_length(quoted);
    if( stackAllocatedString == NULL ) {
        returnValue->alloc = (void*)CS_SB_freeButReturnBuffer(quoted);
        returnValue->typeEnum = CS_JSON_STRING_QUOTED_ALLOCATED;
        returnValue->stringValue = NULL;
    } else {
        CS_SB_free(quoted);
        returnValue->alloc = NULL;
        returnValue->typeEnum = CS_JSON_STRING_QUOTED;
        returnValue->stringValue = stackAllocatedString;
    }
    return returnValue;
}

struct CS_JsonNode *CS_jsonNodeAppendUnquotedString( struct CS_JsonNode *appendTo, const char *name, const char *value ) {
    struct CS_StringBuilder *quoted = CS_quoteString( value, strlen(value) );
    if( quoted == NULL ) return NULL;
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue == NULL ) {
        CS_SB_free(quoted);
        return NULL;
    }
    return stuffQuotedSBOntoJsonNodeAndFreeSB( returnValue, quoted );
}
struct CS_JsonNode *CS_jsonNodeAppendFloat( struct CS_JsonNode *appendTo, const char *name, double value ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue == NULL ) return NULL;
    returnValue->typeEnum = CS_JSON_FLOAT;
    returnValue->floatValue = value;
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAppendInteger( struct CS_JsonNode *appendTo, const char *name, long long value ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue == NULL ) return NULL;
    returnValue->typeEnum = CS_JSON_INTEGER;
    returnValue->intValue = value;
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAppendFloatAsString( struct CS_JsonNode *appendTo, const char *name, const char *value ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue == NULL ) {
        return NULL;
    }
    const char *stackAllocatedString = privateAllocString(appendTo,value);
    if( stackAllocatedString == NULL ) {
        CS_jsonNodeRemoveNode( returnValue );
        return NULL;
    }
    returnValue->typeEnum = CS_JSON_FLOAT_AS_STRING;
    returnValue->stringValue = stackAllocatedString;
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAppendIntegerAsString( struct CS_JsonNode *appendTo, const char *name, const char *value ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue == NULL ) {
        return NULL;
    }
    const char *stackAllocatedString = privateAllocString(appendTo,value);
    if( stackAllocatedString == NULL ) {
        CS_jsonNodeRemoveNode( returnValue );
        return NULL;
    }
    returnValue->typeEnum = CS_JSON_INTEGER_AS_STRING;
    returnValue->stringValue = stackAllocatedString;
    return returnValue;return NULL;
}
struct CS_JsonNode *CS_jsonNodeAppendQuotedString( struct CS_JsonNode *appendTo, const char *name, const char *value ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue == NULL ) {
        return NULL;
    }
    const char *stackAllocatedString = privateAllocString(appendTo,value);
    int nBytes = strlen(value);
    returnValue->nItemsOrLength = nBytes;
    if( stackAllocatedString == NULL ) {
        char *newBuff = CS_alloc(nBytes+1);
        if( newBuff == NULL ) {
            CS_jsonNodeRemoveNode( returnValue );
            return NULL;
        }
        strncpy(newBuff, value, nBytes+1);
        returnValue->alloc = newBuff;
        returnValue->stringValue = NULL;
        returnValue->typeEnum = CS_JSON_STRING_QUOTED_ALLOCATED;
    } else {
        returnValue->typeEnum = CS_JSON_STRING_QUOTED;;
        returnValue->stringValue = stackAllocatedString;
        returnValue->alloc = NULL;
    }
    return returnValue;
}

struct CS_JsonNode *CS_jsonNodeNew( int allocSize ) {
    return privateInitJson( allocSize );
}
struct CS_JsonNode *CS_jsonNodeAppendObject( struct CS_JsonNode *appendTo, const char *name ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name ); 
    if( returnValue != NULL ) {
        returnValue->typeEnum = CS_JSON_OBJECT;
    }
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAppendArray( struct CS_JsonNode *appendTo, const char *name ) {
    struct CS_JsonNode *returnValue = privateAppend( appendTo, name );
    if( returnValue != NULL ) {
        returnValue->typeEnum = CS_JSON_ARRAY;
    }
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAddUnquotedString( struct CS_JsonNode *addTo, const char *name, const char *value ) {
    struct CS_StringBuilder *quoted = CS_quoteString( value, strlen(value) );
    if( quoted == NULL ) return NULL;
    struct CS_JsonNode *returnValue = privateAdd( addTo, name );
    if( returnValue == NULL ) {
        CS_SB_free( quoted );
        return NULL;
    }
    return stuffQuotedSBOntoJsonNodeAndFreeSB( returnValue, quoted );
}
struct CS_JsonNode *CS_jsonNodeAddFloat( struct CS_JsonNode *addTo, const char *name, double value ) {
    struct CS_JsonNode *returnValue = privateAdd( addTo, name );
    if( returnValue != NULL ) {
        returnValue->typeEnum = CS_JSON_FLOAT;
        returnValue->floatValue = value;
    }
    return returnValue;
}

struct CS_JsonNode *CS_jsonNodeAddInteger( struct CS_JsonNode *addTo, const char *name, long long value ) {
    struct CS_JsonNode *returnValue = privateAdd( addTo, name );
    if( returnValue != NULL ) {
        returnValue->typeEnum = CS_JSON_INTEGER;
        returnValue->intValue = value;
    }
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAddObject( struct CS_JsonNode *addTo, const char *name ) {
    struct CS_JsonNode *returnValue = privateAdd( addTo, name );
    if( returnValue != NULL ) {
        returnValue->typeEnum = CS_JSON_OBJECT;
    }
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeAddArray( struct CS_JsonNode *addTo, const char *name ) {
    struct CS_JsonNode *returnValue = privateAdd( addTo, name );
    if( returnValue != NULL ) {
        returnValue->typeEnum = CS_JSON_ARRAY;
    }
    return returnValue;
}
struct CS_JsonNode *CS_jsonNodeToUnquoted( struct CS_JsonNode *in, bool followTree ) {
    struct CS_JsonNode *current = in;
    int newLength;
    long long newInteger;
    double newFloat;
    char *endOfValue;
    while( current ) {
        CS_LOG_TRACE("Started with %s", CS_jsonEnumTypeAsString( current->typeEnum ));
        switch( current->typeEnum ) {
            case CS_JSON_ARRAY:
            case CS_JSON_OBJECT:
                if( !followTree ) continue;
                if( current->container != NULL ) {
                    current = current->container;
                    continue;
                }
                break;
            case CS_JSON_STRING_QUOTED:
                newLength = unquotePrivate( (char*)current->stringValue, (char*)current->stringValue, current->nItemsOrLength );
                if( newLength < 0 ) {
                    CS_LOG_ERROR("Failed to unquote the string.");
                    return NULL;
                }
                current->typeEnum = CS_JSON_STRING_UNQUOTED;
                current->nItemsOrLength = newLength;
                break;
            case CS_JSON_STRING_QUOTED_ALLOCATED:
                newLength = unquotePrivate( (char*)current->alloc, current->alloc, current->nItemsOrLength );
                if( newLength < 0 ) {
                    CS_LOG_ERROR("Failed to unquote the string.");
                    return NULL;
                }
                current->typeEnum = CS_JSON_STRING_UNQUOTED_ALLOCATED;
                current->nItemsOrLength = newLength;
                break;
            case CS_JSON_INTEGER_AS_STRING:
                newInteger = strtoll( current->stringValue, &endOfValue, 10 );
                if( endOfValue == current->stringValue ) {
                    CS_LOG_ERROR( "We couldn't make heads or tails out of the integer saved as a string." );
                    return NULL;
                }
                if( newInteger == LLONG_MIN || newInteger == LLONG_MAX ) {
                    if( errno == ERANGE ) {
                        CS_LOG_WARN( "Attempt to parse an integer is going to over/underflow." );
                        return NULL;
                    }
                }
                current->intValue = newInteger;
                current->typeEnum = CS_JSON_INTEGER;
                break;
            case CS_JSON_FLOAT_AS_STRING:
                newFloat = strtod( current->stringValue, &endOfValue );
                if( current->stringValue == endOfValue ) {
                    CS_LOG_ERROR("We couldn't make heads or tails out of the float saved as a string.");
                    return NULL;
                }
                if( newFloat == HUGE_VALF || newFloat == 0.0 ) {
                    if( errno == ERANGE ) {
                        CS_LOG_ERROR( "Attempt to parse a float is going to over/underflow.");
                        break;
                    }
                }
                current->floatValue = newFloat;
                current->typeEnum = CS_JSON_FLOAT;
                break;
            default:
                break;
        }
        CS_LOG_TRACE("Ended with %s", CS_jsonEnumTypeAsString( current->typeEnum ));
        
        if( !followTree ) break;
        if( current->next ) {
            current = current->next;
        } else {
            while( current != NULL && current->next == NULL ) current = current->up;
            if( current ) current = current->next;
        }
    }
    return in;
}
