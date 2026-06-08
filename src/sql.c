#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <sqlite3.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>
#include <crankshaft/linearalloc.h>

#include <crankshaft/sql.h>


struct CS_SqlBackend *CS_sqlInit( const struct CS_SqlBackendDefinition *definition, const void *data ) {
    struct CS_SqlBackend *returnValue = CS_allocZero( sizeof(struct CS_SqlBackend) );

    if( returnValue ) {
        returnValue->backendDefinition = definition;
        if( definition->init( returnValue, data ) ) {
            CS_free( returnValue );
            returnValue = NULL;
        }
    }

    return returnValue;
}

bool CS_sqlClose( struct CS_SqlBackend *backend ) {
    return backend->backendDefinition->kill( backend );
}

struct CS_SqlResponse *CS_sqlQuery( struct CS_SqlBackend *backend, const struct CS_String *query ) {
    return backend->backendDefinition->runSql( backend, query );
}

void CS_sqlReturnResponse( struct CS_SqlResponse *response ) {
    if( response ) {
        if( response->responseAllocator ) CS_linearFree( response->responseAllocator);
        response->responseAllocator = NULL;
        CS_free( response );
    }
}

void privateReturnResponse( struct CS_SqlResponse *toReturn ) {
    if( toReturn ) {
        if( toReturn->responseAllocator ) {
            CS_linearFree( toReturn->responseAllocator );
        }
        CS_free(toReturn);
    }
}

struct CS_SqlResponse *privateGetResponse() {
    struct CS_SqlResponse *returnValue = CS_allocZero(sizeof(struct CS_SqlResponse));
    if( returnValue == NULL ) return NULL;

    returnValue->responseAllocator = CS_linearInit( 4096 );

    if( returnValue->responseAllocator == NULL ) {
        CS_free(returnValue);
        returnValue = NULL;
    }

    return returnValue;
}

/*
struct CS_SqlBackendDefinition {
    const struct CS_String *name;
    bool (*init)( struct CS_SqlBackend *backend, const void *config );
    bool (*kill)( struct CS_SqlBackend *backend );
    struct CS_SqlResponse *(*runSql)( struct CS_SqlBackend *backend, const struct CS_String *sql );
};
*/

struct _sqliteInfo {
    sqlite3 *connection;
};

static bool _sqliteInit( struct CS_SqlBackend *output, const void *config ) {
    const struct CS_SqlSQLITEInitData *initConfig = (const struct CS_SqlSQLITEInitData *)config;

    struct _sqliteInfo *info = CS_allocZero(sizeof(struct _sqliteInfo));

    if( !info ) {
        goto ERR_INFO;
    }

    output->backendData = info;

    if( sqlite3_open_v2( initConfig->filename, &info->connection, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK ) goto ERR_OPEN;

    return false;
ERR_OPEN:
    output->backendData = NULL;
    CS_free(info);
ERR_INFO:
    return true;
}

static bool _sqliteKill( struct CS_SqlBackend *backend ) {
    if( !backend ) return true;
    if( backend->backendData ) {
        struct _sqliteInfo *info = (struct _sqliteInfo *)backend->backendData;
        sqlite3_close( info->connection );
        CS_free( backend->backendData );
    }
    return false;
}

static struct CS_SqlRow *privateRow( sqlite3_stmt *statement, int32_t numColumns, struct CS_LinearAllocator *allocator ) {
    struct CS_SqlRow *returnValue = CS_linearTakeZero( allocator, sizeof(struct CS_SqlRow), sizeof(void*) );
    if( returnValue ) {
        returnValue->values = CS_linearTakeZero( allocator, sizeof( struct CS_SqlValue ) * numColumns, sizeof( void * ) );
    }
    if( returnValue->values == NULL ) return NULL;
    for( int i = 0; i < numColumns; ++i ) {
        int columnType = sqlite3_column_type(statement, i);
        switch( columnType ) {
            case SQLITE_INTEGER:
                returnValue->values[i].intValue = sqlite3_column_int64(statement, i);
                returnValue->values[i].type = CS_SQL_VALUE_INT;
                break;
            case SQLITE_FLOAT:
                returnValue->values[i].floatValue = sqlite3_column_double(statement, i);
                returnValue->values[i].type = CS_SQL_VALUE_FLOAT;
                break;
            case SQLITE_TEXT:
                returnValue->values[i].stringValue = CS_stringLinearCopyCstring( (const char*)sqlite3_column_text(statement, i), -1, allocator);
                returnValue->values[i].type = CS_SQL_VALUE_STRING;
                break;
            case SQLITE_BLOB:
                returnValue->values[i].stringValue = CS_stringLinearCopyCstring( (const char*)sqlite3_column_blob(statement, i), sqlite3_column_bytes(statement, i), allocator );
                returnValue->values[i].type = CS_SQL_VALUE_BLOB;
                break;
            case SQLITE_NULL:
                returnValue->values[i].intValue = 0;
                returnValue->values[i].type = CS_SQL_VALUE_NULL;
                break;
        }
    }
    return returnValue;
}

static struct CS_SqlResponse *_sqliteRunSql( struct CS_SqlBackend *backend, const struct CS_String *sql ) {
    struct _sqliteInfo *info = (struct _sqliteInfo *)backend->backendData;
    struct CS_SqlResponse *returnValue = privateGetResponse();
    if( returnValue == NULL ) return NULL;
    sqlite3_stmt *statement;
    int prepareReturn = sqlite3_prepare_v2( info->connection, sql->data, sql->length, &statement, NULL );
    if( prepareReturn != SQLITE_OK ) {
        CS_LOG_ERROR( "sqlite3_prepare_v2: %s", sqlite3_errmsg(info->connection) );
        privateReturnResponse(returnValue);
        return NULL;
    }
    int32_t numColumns = sqlite3_column_count(statement);
    returnValue->numColumns = numColumns;
    returnValue->columnNames = CS_linearTakeZero( returnValue->responseAllocator, sizeof(struct CS_String *) * numColumns, sizeof(void*) );
    for( int i = 0; i < numColumns; ++i ) {
        returnValue->columnNames[i] = CS_stringLinearCopyCstring(sqlite3_column_name(statement, i),-1,returnValue->responseAllocator);
    }
    struct CS_SqlRow *currentRow = NULL;
    struct CS_SqlRow *lastRow = NULL;
    int stepReturn;
    bool running = true;
    while(running) {
        stepReturn = sqlite3_step( statement );
        switch( stepReturn ) {
            case SQLITE_ROW:
                currentRow = privateRow(statement, numColumns, returnValue->responseAllocator );
                if( currentRow == NULL ) {
                    running = false;
                    privateReturnResponse( returnValue );
                    returnValue = NULL;
                }
                returnValue->numRows++;
                if( lastRow ) lastRow->next = currentRow;
                lastRow = currentRow;
            break;
            case SQLITE_DONE:
                running = false;
                break;
            case SQLITE_BUSY:
            case SQLITE_ERROR:
            case SQLITE_MISUSE:
                CS_LOG_ERROR( "sqlite3_step: %s", sqlite3_errmsg(info->connection) );
                running = false;
                break;
            default:
                break;
        }
    }
    return returnValue;
}

static const struct CS_String SQLITE = CS_STRING("SQLITE");
struct CS_SqlBackendDefinition CS_SQL_SQLITE = {
    &SQLITE,
    _sqliteInit,
    _sqliteKill,
    _sqliteRunSql
};


