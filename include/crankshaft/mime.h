#ifndef __crankshaftmimedoth__
#define __crankshaftmimedoth__
#include <stdbool.h>

/********************************************************************
 *
 * MIME types handling for http handling
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include <crankshaft/mimeenum.h>
#include <stdint.h>

int32_t CS_mimeFileExtensionToEnum(const char *extension );
const char *CS_mimeFileExtensionToString(const char *extension);
const char *CS_mimeEnumToString(int32_t mimeEnum);
 
#ifdef __cplusplus
}
#endif
#endif
