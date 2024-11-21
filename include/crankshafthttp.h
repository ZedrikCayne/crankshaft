#ifndef __crankshafthttpdoth__
#define __crankshafthttpdoth__
#include <stdbool.h>
#include <openssl/ssl.h>

#include "crankshaftpushpull.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CS_MAX_REQUEST_LENGTH 4096
struct CS_HttpRequest {
    char address[ CS_MAX_REQUEST_LENGTH ];
    struct CS_PushPullBuffer *buffIn;
    struct CS_PushPullBuffer *buffOut;
    int ioSocket;
    SSL *ssl;
};



#ifdef __cplusplus
}
#endif
#endif
