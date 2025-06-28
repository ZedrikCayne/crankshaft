#ifndef __crankshaftmutexdoth__
#define __crankshaftmutexdoth__
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

pthread_mutex_t *CS_mutexGrab();
void CS_mutexReturn(pthread_mutex_t *returnMe);


#ifdef __cplusplus
}
#endif
#endif
