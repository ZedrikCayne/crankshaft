#ifndef __crankshaftuuiddoth__
#define __crankshaftuuiddoth__
#include <stdbool.h>

/********************************************************************
 *
 * Generic UUID handling, deals with both the textual and binary
 * versions of type 4 UUIDs of the form
 * XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
 *
 * Functions ending in Temp return values in the global temp buffer
 * that do not require freeing. Functions ending in Out require
 * you to have a buffer for it to write to.
 *
 * Any others you should free using CS_uuidFreeString or CS_uuidFree
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define UUID_CHAR_SIZE_BYTES 37
#define UUID_BYTES_IN_UUID 16

struct CS_UUID {
    const unsigned char uuid[ UUID_BYTES_IN_UUID ];
};
void CS_uuidInit(void);
void CS_uuidKill(void);
void CS_uuidSetSeed( int seed );
const struct CS_UUID *CS_uuid4();
const struct CS_UUID *CS_uuid4Temp(void);
void CS_uuidFree(const struct CS_UUID *uuid);
const char *CS_uuid4StringTemp(void);
const char *CS_uuid4String(void);
const char *CS_uuid4StringOut(char *out, int length);
const struct CS_UUID *CS_uuidFromString(char *in, int length);
const struct CS_UUID *CS_uuidFromStringTemp(char *in, int length);
const char *CS_uuidToString(const struct CS_UUID *uuid);
const char *CS_uuidToStringTemp(const struct CS_UUID *uuid);
const char *CS_uuidToStringOut(const struct CS_UUID *uuid, char *out, int outLength);
void CS_uuidFreeString( const char *uuidString );
void CS_uuidCopy( struct CS_UUID *dest, const struct CS_UUID *source );

#ifdef __cplusplus
}
#endif
#endif
