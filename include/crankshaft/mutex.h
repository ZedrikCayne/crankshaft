#ifndef __crankshaftmutexdoth__
#define __crankshaftmutexdoth__
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUTEX_MAX_NAME 32
struct CS_Mutex;

void CS_mutexDebug( bool logDebug );

#define CS_mutexTake() CS_mutexTakeDetailled( NULL, __FILE__, __LINE__ )
#define CS_mutexTakeNamed(__NAME) CS_mutexTakeDetailled( __NAME, __FILE__, __LINE__ )
#define CS_mutexReturn(__MUTEX) CS_mutexReturnDetailled( __MUTEX, __FILE__, __LINE__ )
#define CS_mutexLock(__MUTEX) CS_mutexLockDetailled( __MUTEX, 0, __FILE__, __LINE__ )
#define CS_mutexUnlock(__MUTEX) CS_mutexUnlockDetailled( __MUTEX, __FILE__, __LINE__ )
#define CS_mutexLockTimed(__MUTEX,__MSEC) CS_mutexLockDetailled( __MUTEX, __MSEC, __FILE__, __LINE__ );

struct CS_Mutex *CS_mutexTakeDetailled( const char *name, const char *file, int line );
bool CS_mutexReturnDetailled(struct CS_Mutex *returnMe,const char *file, int line);
bool CS_mutexLockDetailled(struct CS_Mutex *toLock,int msec, const char *file, int line);
bool CS_mutexUnlockDetailled(struct CS_Mutex *toUnlock,const char *file, int line);

#ifdef __cplusplus
}
#endif
#endif
