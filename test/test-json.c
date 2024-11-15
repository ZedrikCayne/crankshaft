#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "crankshaftalloc.h"
#include "crankshaftlogger.h"
#include "crankshafttest.h"
#include "crankshafttempbuff.h"
#include "crankshaftjson.h"

extern bool test_json();

static int testCount = 0;
static int testSucceeded = 0;

static const char *testsource1 = "[\"foo\",\"bar\",\"baz\"]";
static const char *testsource2 = "{\"a\":\"1\",\"b\":2,\"c\":3.3}";

#define ARRAY_LENGTH(X) (sizeof(X)/(sizeof(X[0])))

static const char *validIntegers[] = {
    "1",
    "0",
    "2",
    "3",
    "99999999999999992828222"
};

static const char *invalidIntegers[] = {
    "00",
    "",
    ".",
    "a",
};

static const char *invalidIntegersButValidToken[] = {
    "0.1",
    "1.0",
    "1e1",
    "\"\""
};

static const char *validFloat[] = {
    "0.1",
    "1.0",
    "1.0e1",
    "1.0e1.0",
    "1.0E1",
    "2.0E1.0",
    "1.0e0",
    "-1.23e0.1",
    "-1.23e+3",
    "-1.33e+3.0",
    "-1.45E-3.0",
};

static const char *invalidFloat[] = {
    "2.03.00",
    "00.1234",
    "1e1.02e1",
    "1.34e-5.6.4",
    "+1.356",
    "-1.3e+2.13-e",
    "-1e-1e1",
    "-1ee",
    "e",
    "1e-1ee",
    "13.",
    ".001",
    "0.00e.01",
};

#define TEMP_BUFF_SIZE 2048
#define TEMP_BUFF_COUNT 20
#define TEMP_BUFF_ALIGNMENT 4
#define TEMP_JSON_ALLOC_SIZE 512
#define TEMP_JSON_ALLOC_SIZE_BIG 8192 
#define TEMP_PRINT_SIZE 512

static const char *wantedButGot(int wantedEnum, struct CS_JsonNode *got) {
    char *returnValue = CS_tempBuff( TEMP_PRINT_SIZE );
    snprintf(returnValue, TEMP_PRINT_SIZE, "Wanted a %s but got %s.", CS_jsonEnumTypeAsString( wantedEnum ),
            got?CS_jsonEnumTypeAsString( got->typeEnum ):"NULL" );
    return returnValue;
}

static const char *nullButGot(struct CS_JsonNode *got) {
    char *returnValue = CS_tempBuff( TEMP_PRINT_SIZE );
    snprintf(returnValue, TEMP_PRINT_SIZE, "Wanted a NULL but got %s.", got?CS_jsonEnumTypeAsString( got->typeEnum ):"NULL" );
    return returnValue;
}

#define PARSE_N_PRINT(x) __temp=CS_tempBuff(TEMP_PRINT_SIZE);snprintf(__temp,TEMP_PRINT_SIZE,"Parse \"%s\"",x[i]);js=CS_parseJsonCopy(x[i],strlen(x[i]),TEMP_JSON_ALLOC_SIZE)
#define SET__TEMP(x) __temp=CS_tempBuff(TEMP_PRINT_SIZE);snprintf(__temp,TEMP_PRINT_SIZE,"Parse \"%s\"",x)

struct CS_JsonNode *addRandom( struct CS_JsonNode *to, int index ) {
    struct CS_JsonNode *current = to;
    struct CS_JsonNode *countUpNode = current;
    bool addIfTrue = (to->typeEnum==CS_JSON_OBJECT) || (to->typeEnum==CS_JSON_ARRAY);
    bool needName = false;
    int typeToAdd = 0;
    int countUp = 0;
    bool goUpFirst = false;
    char *name = NULL;
    char *value = NULL;

    goUpFirst = ((to->up!=NULL)&&(to->up->up!=NULL)) && (CS_testRandMax(4)==0);
    if( goUpFirst ) {
        addIfTrue = false;
        current = current->up;
    }
    needName = addIfTrue?current->typeEnum==CS_JSON_OBJECT:(current->up)&&(current->up->typeEnum==CS_JSON_OBJECT);
    if( needName ) {
        name = CS_tempBuff(128);
        snprintf( name, 128, "KEY%d", index );
    } else {
        name = NULL;
    }
    value = CS_tempBuff(128);
    countUp = 0; countUpNode = current;
    while( countUpNode ) {
        countUpNode = countUpNode->up;
        ++countUp;
    }
    
    snprintf( value, 128, "V %d, %d", index, countUp );

    typeToAdd = CS_testRandMax(5);
    if( addIfTrue ) {
        switch(typeToAdd) {
            case 0:
                current = CS_jsonNodeAddUnquotedString(current, name, value);
                break;
            case 1:
                current = CS_jsonNodeAddInteger(current, name, index);
                break;
            case 2:
                current = CS_jsonNodeAddFloat(current, name, index + (0.001 * countUp));
                break;
            case 3:
                current = CS_jsonNodeAddObject(current, name);
                break;
            case 4:
                current = CS_jsonNodeAddArray( current, name );
                break;
        }
    } else {
        switch(typeToAdd) {
            case 0:
                current = CS_jsonNodeAppendUnquotedString(current, name, value);
                break;
            case 1:
                current = CS_jsonNodeAppendInteger(current, name, index);
                break;
            case 2:
                current = CS_jsonNodeAppendFloat(current, name, index + (0.001 * countUp));
                break;
            case 3:
                current = CS_jsonNodeAppendObject(current, name);
                break;
            case 4:
                current = CS_jsonNodeAppendArray( current, name );
                break;
        }
    }
    if( current == NULL ) CS_LOG_ERROR( "WAHT!" );
    return current;
}

//char *firstTest = "1.0e1";
char *firstTest = "{\"a\":[]}";

bool test_json() {
    char *__temp = NULL;
    //Tests go here:
    void *myTempBuffer = CS_allocManualTempBuff( "TEST BUFF", TEMP_BUFF_SIZE, TEMP_BUFF_COUNT, TEMP_BUFF_ALIGNMENT );

    char *t0 = CS_getTempBuff( myTempBuffer );
    strncpy( t0, firstTest, TEMP_BUFF_SIZE );
    SET__TEMP(firstTest);
    struct CS_JsonNode * js = CS_parseJsonCopy( t0, strlen( t0 ), TEMP_JSON_ALLOC_SIZE);
    CS_FAIL_ON_FALSE( js && js->typeEnum == CS_JSON_OBJECT, __temp, "%s", wantedButGot( CS_JSON_FLOAT_AS_STRING, js ) );
    if( js ) {
        CS_freeJson(js);
    }

    char *t1 = CS_getTempBuff( myTempBuffer );

    strncpy(t1, testsource1, TEMP_BUFF_SIZE);

    js = CS_parseJsonCopy( testsource1, strlen( t1 ), TEMP_JSON_ALLOC_SIZE);
    
    CS_FAIL_ON_FALSE( js && js->typeEnum == CS_JSON_ARRAY, "Parse an array", "%s", wantedButGot( CS_JSON_ARRAY, js ) );
    if( js ) CS_freeJson(js);

    char *t2 = CS_getTempBuff( myTempBuffer );

    strncpy(t2, testsource2, TEMP_BUFF_SIZE);

    js = CS_parseJsonCopy( testsource2, strlen( t2 ), TEMP_JSON_ALLOC_SIZE );
    CS_FAIL_ON_FALSE( js && js->typeEnum == CS_JSON_OBJECT, "Parse an object.", "%s", wantedButGot( CS_JSON_OBJECT, js ) );
    if( js ) CS_freeJson(js);
    for( int i = 0; i < ARRAY_LENGTH( validIntegers ); ++i ) {
        PARSE_N_PRINT(validIntegers);
        CS_FAIL_ON_FALSE( js && js->typeEnum == CS_JSON_INTEGER_AS_STRING, __temp, "%s", wantedButGot(CS_JSON_INTEGER_AS_STRING, js) );
        if( js ) CS_freeJson(js);
    }
    
    for( int i = 0; i < ARRAY_LENGTH( invalidIntegers ); ++i ) {
        PARSE_N_PRINT(invalidIntegers);
        CS_FAIL_ON_NOT_NULL( js, __temp, "%s", nullButGot( js ) );
        if( js ) CS_freeJson(js);
    }

    for( int i = 0; i < ARRAY_LENGTH( invalidIntegersButValidToken ); ++i ) {
        PARSE_N_PRINT(invalidIntegersButValidToken);
        CS_FAIL_ON_NULL( js, __temp, "Wanted a valid js node but instead got a null." );
        CS_FAIL_ON_TRUE( js && js->typeEnum == CS_JSON_INTEGER_AS_STRING, __temp, "%s", wantedButGot( CS_JSON_ERROR, js ) );
        if( js ) CS_freeJson(js);
    }

    for( int i = 0; i < ARRAY_LENGTH( validFloat ); ++i ) {
        PARSE_N_PRINT(validFloat);
        CS_FAIL_ON_FALSE( js && js->typeEnum == CS_JSON_FLOAT_AS_STRING, __temp, "%s", wantedButGot(CS_JSON_FLOAT_AS_STRING, js ) );
        if( js ) CS_freeJson(js);
    }

    for( int i = 0; i < ARRAY_LENGTH( invalidFloat ); ++i ) {
        PARSE_N_PRINT(invalidFloat);
        CS_FAIL_ON_NOT_NULL( js, __temp, "%s", nullButGot( js ) );
        if( js ) CS_freeJson(js);
    }

    js = CS_jsonNodeNew( TEMP_JSON_ALLOC_SIZE );
    struct CS_JsonNode *root = js;
    js = CS_jsonNodeAppendInteger(js, NULL, 5);

    CS_FAIL_ON_FALSE( root == js, "Initial append integer.", "First 'node' appended should also be the root node." );
    CS_FAIL_ON_FALSE( js->typeEnum == CS_JSON_INTEGER, "Appended object type check.", "%s", wantedButGot( CS_JSON_INTEGER, js ) );
    
    CS_FAIL_ON_NOT_NULL( js = CS_jsonNodeAppendFloat(js, NULL, 1.0), "Adding to a root node should fail.", "%s", nullButGot( js ) );

    if( root ) CS_freeJson( root );

    root = js = CS_jsonNodeNew( TEMP_JSON_ALLOC_SIZE );
    js = CS_jsonNodeAppendFloat(js, NULL, 1.0);
    CS_FAIL_ON_FALSE( root == js, "Initial append float.", "First 'node' appended should also be the root node." );
    CS_FAIL_ON_FALSE( js->typeEnum == CS_JSON_FLOAT, "Appended object type check.", "%s", wantedButGot( CS_JSON_FLOAT, js ) );
    CS_FAIL_ON_NOT_NULL( js = CS_jsonNodeAppendInteger(js, NULL, 5), "Adding to a root node should fail.", "%s", nullButGot( js ) );
    if( root ) CS_freeJson( root );

    root = js = CS_jsonNodeNew( TEMP_JSON_ALLOC_SIZE );

    t1 = CS_getTempBuff( myTempBuffer );
    for( int i = 0; i < TEMP_BUFF_SIZE; ++i ) {
        t1[ i ] = (i % 126) + 1;
    }
    t1[TEMP_BUFF_SIZE-1] = 0;
    js = CS_jsonNodeAppendUnquotedString( js, NULL, t1 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_STRING_QUOTED_ALLOCATED, "Adding a very long string should allocate it.", "%s", wantedButGot( CS_JSON_STRING_QUOTED_ALLOCATED, js ) );
    if( root ) CS_freeJson( root );

    root = js = CS_jsonNodeNew( TEMP_JSON_ALLOC_SIZE_BIG );
    js = CS_jsonNodeAppendUnquotedString( js, NULL, t1 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_STRING_QUOTED, "Adding a very long string to a sufficienlty sized allocator should not allocate buffers for the string.", "%s", wantedButGot( CS_JSON_STRING_QUOTED, js ) );
    if( root ) CS_freeJson( root );

    root = js = CS_jsonNodeNew( TEMP_JSON_ALLOC_SIZE_BIG );
    js = CS_jsonNodeAppendArray( js, NULL );
    CS_FAIL_ON_FALSE( root == js, "Initial add should be the root node.", "Initial add is not the root node." );
    js = CS_jsonNodeAddInteger( js, NULL, 1 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_INTEGER, "Type correct 1", "%s", wantedButGot( CS_JSON_INTEGER, js ) );
    js = CS_jsonNodeAppendFloat( js, NULL, 2.0 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_FLOAT, "Type correct 2.0", "%s", wantedButGot( CS_JSON_FLOAT, js ) );
    js = CS_jsonNodeAppendInteger( js, NULL, 3 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_INTEGER, "Type correct 3", "%s", wantedButGot( CS_JSON_INTEGER, js ) );
    js = CS_jsonNodeAppendFloat( js, NULL, 4.0 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_FLOAT, "Type correct 4.0", "%s", wantedButGot( CS_JSON_FLOAT, js ) );
    js = CS_jsonNodeAppendInteger( js, NULL, 5 );
    CS_FAIL_ON_FALSE( js != NULL && js->typeEnum == CS_JSON_INTEGER, "Type correct 5", "%s", wantedButGot( CS_JSON_INTEGER, js ) );
    static const char *equivalent = "[1,2.0,3,4.0,5]";
    struct CS_JsonNode *js2 = CS_parseJsonCopy( equivalent, strlen( equivalent ), TEMP_JSON_ALLOC_SIZE_BIG );
    CS_FAIL_ON_FALSE( js2 == CS_jsonNodeToUnquoted( js2, true ), "Conversion from strings to binary/unquoted.", "Should have been equivalent" );
    CS_FAIL_ON_FALSE( CS_jsonNodesEquivalent( root, js2 ), "These two should have ended up equivalent.", "Oops." );
    if( js2 != NULL ) CS_freeJson( js2 );
    static const char *notEquivalent = "[1,2.0,3,4.0,4]";
    js2 = CS_parseJsonCopy( notEquivalent, strlen( equivalent ), TEMP_JSON_ALLOC_SIZE_BIG );
    CS_FAIL_ON_FALSE( js2 == CS_jsonNodeToUnquoted( js2, true ), "Conversion from strings to binary/unquoted.", "Should have been equivalent" );
    CS_FAIL_ON_TRUE( CS_jsonNodesEquivalent( root, js2 ), "These two should have ended up different.", "Oops." );
    if( js2 != NULL ) CS_freeJson( js2 );
    if( root != NULL ) CS_freeJson( root );

    for( int i = 0; i < 25; ++i ) {
        root = js = CS_jsonNodeNew( TEMP_JSON_ALLOC_SIZE_BIG );
        js = CS_jsonNodeAppendObject( js, NULL );
        for( int j = 0; j < 50; ++j ) {
            js = addRandom( js, j );
        }
        struct CS_StringBuilder *out = CS_jsonNodePrintable( root );
        root = CS_jsonNodeToUnquoted( root, true );
        struct CS_StringBuilder *out2 = CS_jsonNodePrintable( root );
        js2 = CS_parseJsonCopy( out->buffer, CS_SB_length( out ), TEMP_JSON_ALLOC_SIZE_BIG );
        js2 = CS_jsonNodeToUnquoted( js2, true );
        struct CS_StringBuilder *out3 = CS_jsonNodePrintable(js2);
        CS_FAIL_ON_FALSE( CS_jsonNodesEquivalent( root, js2 ), "Should be equivalent", "\n%s\n%s\n%s", out->buffer,out2->buffer,out3->buffer );
        CS_SB_free( out );
        CS_SB_free( out2 );
        CS_SB_free( out3 );
        CS_freeJson( js2 );
        CS_freeJson( root );
    }
    


    CS_freeManualTempBuff( myTempBuffer );

    return testCount !=
           testSucceeded;
}


