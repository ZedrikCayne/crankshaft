#ifndef __crankshaftssldoth__
#define __crankshaftssldoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool CS_sslInit();
bool CS_sslKill();

SSL *CS_sslNew( bool tlsV1 );

#ifdef __cplusplus
}
#endif
#endif
