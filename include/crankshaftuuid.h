#ifndef __crankshaftuuiddoth__
#define __crankshaftuuiddoth__
#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif
#endif
