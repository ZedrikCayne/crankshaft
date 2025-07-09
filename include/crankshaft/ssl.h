#ifndef __crankshaftssldoth__
#define __crankshaftssldoth__
#include <stdbool.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

bool CS_sslInit( const char *keyFile, const char *certFile, const char *selfSignHostname );
bool CS_sslKill();

SSL *CS_sslNew( bool tlsV1 );

#ifdef __cplusplus
}
#endif
#endif
