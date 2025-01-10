#ifndef __crankshaftmimedoth__
#define __crankshaftmimedoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <crankshaft/mimeenum.h>

int CS_mimeFileExtensionToEnum(const char *extension );
const char *CS_mimeFileExtensionToString(const char *extension);
const char *CS_mimeEnumToString(int mimeEnum);
 
#ifdef __cplusplus
}
#endif
#endif
