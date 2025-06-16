#include <stdlib.h>
#include <stdio.h>

#include <openssl/ssl.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/ssl.h>

static bool initedSSL = false;
static bool killCalled = false;

bool CS_sslInit() {
    if( killCalled )
        return true;
    if( !initedSSL ) {
        if( OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS|
                             OPENSSL_INIT_ADD_ALL_CIPHERS|
                             OPENSSL_INIT_ADD_ALL_DIGESTS|
                             OPENSSL_INIT_LOAD_CONFIG,
                             NULL) != 1 ) {
            CS_LOG_ERROR("ssl: OPENSSL_init_ssl fail.");
            return true;
        }
        initedSSL = true;
    }
    return false;
}

bool CS_sslKill() {
    if( killCalled )
        return true;
    killCalled = true;
    if( initedSSL ) {
        OPENSSL_cleanup();
        initedSSL = false;
    }
    return false;
}

