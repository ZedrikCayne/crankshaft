#ifndef __crankshafthashdoth__
#define __crankshafthashdoth__
#include <stdbool.h>
#include <stdint.h>

/********************************************************************
 *
 * Basic hash stuff... not terribly good but good enough.
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

uint32_t CS_hash(const char *str);
uint32_t CS_hashBin(const char *blob, int32_t length);

#ifdef __cplusplus
}
#endif
#endif
