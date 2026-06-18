#ifndef __crankshaftsqldoth__
#define __crankshaftsqldoth__
#include <stdbool.h>
#include <stdint.h>


#include <crankshaft/string.h>
#include <crankshaft/linearalloc.h>

/****************************************************************************
 *
 * Abstraction of sql backends. We'll provide you the SQLITE implementation.
 *
 * Probably MySql at some point and postgres.
 *
 * All date time values are converted to miliseconds since epoch. (not
 * technically correct, but good enough for government work)
 *
 * Binaries will be spit out as CS_String items. CS_String may contain
 * full binary data.
 *
 ****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define CS_SQL_FLAG_OWN_DATA   0x00000001
#define CS_SQL_STATUS_OK       0x00000001
#define CS_SQL_STATUS_CLOSED   0x00000002
#define CS_SQL_STATUS_ERROR    0x00000004

struct CS_SqlBackend {
    int32_t sqlBackendFlags;
    int32_t sqlStatusFlags;
    const struct CS_SqlBackendDefinition *backendDefinition;
    void *backendData;
};

enum CS_sqlValueType {
    CS_SQL_VALUE_UNKNOWN = 0,
    CS_SQL_VALUE_INT,
    CS_SQL_VALUE_FLOAT,
    CS_SQL_VALUE_STRING,
    CS_SQL_VALUE_DATE,
    CS_SQL_VALUE_BLOB,
    CS_SQL_VALUE_BOOL,
    CS_SQL_VALUE_NULL,
};

struct CS_SqlValue {
    union {
        int64_t intValue;
        double floatValue;
        bool boolValue;
        const struct CS_String *stringValue;
    };
    int32_t type;
};

struct CS_SqlRow {
    struct CS_SqlValue *values;
    struct CS_SqlRow *next;
};

struct CS_SqlResponse {
    struct CS_LinearAllocator *responseAllocator;
    struct CS_SqlRow *rows;
    struct CS_String **columnNames;
    int32_t numColumns;
    int32_t numRows;
};

struct CS_SqlBackendDefinition {
    const struct CS_String *name;
    bool (*init)( struct CS_SqlBackend *backend, const void *config );
    bool (*kill)( struct CS_SqlBackend *backend );
    struct CS_SqlResponse *(*runSql)( struct CS_SqlBackend *backend, const struct CS_String *sql );
};

extern struct CS_SqlBackendDefinition CS_SQL_SQLITE;

struct CS_SqlSQLITEInitData {
    const char *filename;
};

struct CS_SqlBackend *CS_sqlInit( const struct CS_SqlBackendDefinition *definition, const void *data );
bool CS_sqlClose( struct CS_SqlBackend *backend );
const struct CS_SqlResponse *CS_sqlQuery( struct CS_SqlBackend *backend, const struct CS_String *query );
void CS_sqlReturnResponse( const struct CS_SqlResponse *response );

#ifdef __cplusplus
}
#endif
#endif
