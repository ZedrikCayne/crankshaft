#include <stdlib.h>
#include <stdio.h>

#include <openssl/ssl.h>

#include <crankshaft/alloc.h>
#include <crankshaft/logger.h>

#include <crankshaft/ssl.h>

static bool initedSSL = false;
static bool killCalled = false;

static SSL_CTX *gSSL_CTX = NULL;
static SSL_CTX *gSSL_TLSV1_CTX = NULL;
static EVP_PKEY *ss_pkey = NULL;
static X509 *ss_X509 = NULL;

bool CS_sslInit( const char *keyFile, const char *certFile, const char *selfSignHostname ) {
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
        gSSL_CTX = SSL_CTX_new( TLS_method() );
        gSSL_TLSV1_CTX = SSL_CTX_new( TLS_method() );
        SSL_CTX_set_security_level( gSSL_TLSV1_CTX, 0 );
        SSL_CTX_set_options(gSSL_CTX, SSL_OP_NO_TICKET);
        SSL_CTX_set_options(gSSL_TLSV1_CTX, SSL_OP_NO_TICKET);
        SSL_CTX_set_session_cache_mode(gSSL_CTX, SSL_SESS_CACHE_OFF);
        SSL_CTX_set_session_cache_mode(gSSL_TLSV1_CTX, SSL_SESS_CACHE_OFF);
        if( !selfSignHostname && keyFile && certFile ) {
            if( SSL_CTX_use_certificate_chain_file( gSSL_CTX, certFile ) <= 0 ) {
                return true;
            }
            if( SSL_CTX_use_PrivateKey_file( gSSL_CTX, keyFile, SSL_FILETYPE_PEM) <= 0 ) {
                return true;
            }
        }
        if( selfSignHostname && !keyFile && !certFile ) {
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
            if( ctx == NULL ) return true;
            if( EVP_PKEY_keygen_init(ctx) <= 0 ) return true;
            if( EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ) return true;
            if( EVP_PKEY_keygen( ctx, &ss_pkey ) <= 0 ) return true;
            EVP_PKEY_CTX_free(ctx);
            ss_X509 = X509_new();
            ASN1_INTEGER_set(X509_get_serialNumber(ss_X509),1);
            X509_gmtime_adj(X509_get_notBefore(ss_X509), 0);
            X509_gmtime_adj(X509_get_notAfter(ss_X509), 31536000L);
            X509_set_pubkey(ss_X509, ss_pkey);
            X509_NAME * name;
            name = X509_get_subject_name(ss_X509);
            X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
                                               (unsigned char *)"US", -1, -1, 0);
            X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
                                               (unsigned char *)"Just Add Hippo Inc.", -1, -1, 0);
            X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                               (unsigned char *)selfSignHostname, -1, -1, 0);
            X509_set_issuer_name(ss_X509, name);
            X509_sign( ss_X509, ss_pkey, EVP_sha256() );
            SSL_CTX_use_certificate( gSSL_CTX, ss_X509 );
            SSL_CTX_use_PrivateKey( gSSL_CTX, ss_pkey );
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
        if( ss_pkey ) {
            EVP_PKEY_free( ss_pkey );
            ss_pkey = NULL;
        }
        if( ss_X509 ) {
            X509_free( ss_X509 );
            ss_X509 = NULL;
        }
        SSL_CTX_free(gSSL_CTX);
        SSL_CTX_free(gSSL_TLSV1_CTX);
        OPENSSL_cleanup();
        initedSSL = false;
    }
    return false;
}

SSL *CS_sslNew( bool tlsV1 ) {
    return SSL_new( tlsV1?gSSL_TLSV1_CTX:gSSL_CTX );
}

