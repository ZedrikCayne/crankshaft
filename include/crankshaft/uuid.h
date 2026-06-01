#ifndef __crankshaftuuiddoth__
#define __crankshaftuuiddoth__
#include <stdbool.h>
#include <stdint.h>

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
 * Any others you should free using CS_uuidFreeCstring or CS_uuidFree
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
void CS_uuidSetSeed( int32_t seed );
const struct CS_UUID *CS_uuid4();
const struct CS_UUID *CS_uuid4Temp(void);
void CS_uuidFree(const struct CS_UUID *uuid);
const char *CS_uuid4CstringTemp(void);
const char *CS_uuid4Cstring(void);
const char *CS_uuid4CstringOut(char *out, int32_t length);
const struct CS_UUID *CS_uuidFromCstring(char *in, int32_t length);
const struct CS_UUID *CS_uuidFromCstringTemp(char *in, int32_t length);
const char *CS_uuidToCstring(const struct CS_UUID *uuid);
const char *CS_uuidToCstringTemp(const struct CS_UUID *uuid);
const char *CS_uuidToCstringOut(const struct CS_UUID *uuid, char *out, int32_t outLength);
void CS_uuidFreeCstring( const char *uuidString );
void CS_uuidCopy( struct CS_UUID *dest, const struct CS_UUID *source );

#ifdef __cplusplus
}
#endif
#endif
