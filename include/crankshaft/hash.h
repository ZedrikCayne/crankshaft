#ifndef __crankshafthashdoth__
#define __crankshafthashdoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned int CS_hash(const char *str);
unsigned int CS_hashBin(const char *blob, int length);

#ifdef __cplusplus
}
#endif
#endif
