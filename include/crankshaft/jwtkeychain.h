#ifndef __crankshaftjwtkeychaindoth__
#define __crankshaftjwtkeychaindoth__
#include <stdbool.h>

#include <openssl/evp.h>

#include <crankshaft/storage.h>
#include <crankshaft/list.h>

/********************************************************************
 *
 * JWT keychain and cache. Caches responses from public key providers
 * and provides keys by 'kid'
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

struct CS_Jwtkeychain;

//Note, this is the backing storage for the fetched public keys.
bool CS_jwtkeychainInit(const struct CS_Storage *backingStorage);
bool CS_jwtkeychainTeardown();

bool CS_jwtkeychainFetchPublicKeys( const char *urlToFetchKeysFrom );

EVP_PKEY *CS_jwtkeychainGetKey( const char *keyId );

struct CS_List *CS_jwtkeychainGetKeyIds();

#ifdef __cplusplus
}
#endif
#endif
