#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/test.h>

#include <crankshaft/sql.h>

extern bool test_sql(void);

static int testCount = 0;
static int testSucceeded = 0;

bool test_sql(void) {
    //Tests go here:
    struct CS_SqlSQLITEInitData initData = {"/tmp/sqldb.tmp"};
    unlink("/tmp/sqldb.tmp");

    struct CS_SqlBackend *sqlite = CS_sqlInit(&CS_SQL_SQLITE, &initData);

    CS_FAIL_ON_NULL(sqlite, "Init sqlite SQL backend.", "Failed!");

    if( sqlite ) {
        struct CS_String createTable = CS_STRING("CREATE TABLE test_table (\
                col1 TEXT(128),\
                col2 TEXT(128),\
                amount INT(20),\
                CONSTRAINT PK_test_table PRIMARY KEY ( col1, col2 )\
                ) WITHOUT ROWID;");
        const struct CS_SqlResponse *response = CS_sqlQuery( sqlite, &createTable );
        CS_FAIL_ON_NULL( response, "Create Table", "Failed!" );
        if( response ) {
            CS_sqlReturnResponse(response);
        }
        CS_sqlClose(sqlite);
    }



    return testCount !=
           testSucceeded;
}


