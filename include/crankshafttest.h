#ifndef __crankshafttestdoth__
#define __crankshafttestdoth__
#include <stdbool.h>

#include "crankshaftlogger.h"
#include "crankshafttempbuff.h"
#include "crankshaftrandom.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool CS_TEST_PRINT_ONLY_ERRORS;

bool CS_testMain(void);
void CS_testSetRandomSeed(int seed);
void CS_testResetGlobalRandSeed();
int  CS_testRand();
int  CS_testRandMax(int max);

#define MAX_TEST_LOG 1023
#define CS_LOG_OK(TESTNAME) if(!CS_TEST_PRINT_ONLY_ERRORS)CS_LOG_LOUD("[ ] %s",TESTNAME)
#define CS_LOG_NOT_OK(TNAME,...) {char *t = CS_tempBuff(MAX_TEST_LOG);snprintf(t,MAX_TEST_LOG,__VA_ARGS__);CS_LOG_LOUD("[X] %s : %s",TNAME,t);}

#define CS_FAIL_ON_NULL(PREDICATE,TESTNAME,...) { testCount++; void *_CS_pPtr = (PREDICATE); if( _CS_pPtr == NULL ) { CS_LOG_NOT_OK(TESTNAME,__VA_ARGS__); } else { CS_LOG_OK(TESTNAME);++testSucceeded; } }
#define CS_FAIL_ON_NOT_NULL(PREDICATE,TESTNAME,...) { testCount++; void *_CS_pPtr = (PREDICATE); if( _CS_pPtr != NULL ) { CS_LOG_NOT_OK(TESTNAME,__VA_ARGS__); } else { CS_LOG_OK(TESTNAME);++testSucceeded; } }
#define CS_FAIL_ON_TRUE(PREDICATE,TESTNAME,...) { testCount++; bool _CS_bBool = (PREDICATE); if( _CS_bBool ) { CS_LOG_NOT_OK(TESTNAME,__VA_ARGS__); } else { CS_LOG_OK(TESTNAME);++testSucceeded; } }
#define CS_FAIL_ON_FALSE(PREDICATE,TESTNAME,...) { testCount++; bool _CS_bBool = (PREDICATE); if( !_CS_bBool ) { CS_LOG_NOT_OK(TESTNAME,__VA_ARGS__); } else { CS_LOG_OK(TESTNAME);++testSucceeded; } }

#ifdef __cplusplus
}
#endif
#endif
