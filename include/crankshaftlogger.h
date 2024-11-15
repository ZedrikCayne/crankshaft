#ifndef __crankshaftloggerdoth__
#define __crankshaftloggerdoth__
#include <stdio.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

extern bool CS_LOG_ERROR_BOOL;
extern bool CS_LOG_QUIET_BOOL;
extern bool CS_LOG_WARN_BOOL;
extern bool CS_LOG_TRACE_BOOL;
extern bool CS_LOG_INFO_BOOL;
extern bool CS_LOG_VERBOSE_BOOL;

#if defined(CRANKSHAFT_NOLOGS)
#define CS_LOG_ERROR(...) {}
#define CS_LOG_ERROR_IF(...
#else
#define CS_LOG_ERROR(...) if(CS_LOG_ERROR_BOOL){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_ERROR_IF(_PREDICATE,...) if(CS_LOG_ERROR_BOOL&&(_PREDICATE)){printf(__VA_ARGS__);printf("\n");}
#endif

#if defined(CRANKSHAFT_NOLOGS) || defined(CRANKSHAFT_ERROR_LOGS_ONLY)
#define CS_LOG_LOUD(...) {}
#define CS_LOG_LOUD_IF(...)
#define CS_LOG_WARN(...) {}
#define CS_LOG_WARN_IF(...)
#define CS_LOG(...) {}
#define CS_LOG_IF(...)
#else
#define CS_LOG_LOUD(...) if(true){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_LOUD_IF(_PREDICATE,...) if(_PREDICATE){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_WARN(...) if(CS_LOG_WARN_BOOL){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_WARN_IF(_PREDICATE,...) if(CS_LOG_WARN_BOOL&&(_PREDICATE)){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG(...) if(!CS_LOG_QUIET_BOOL){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_IF(_PREDICATE,...) if(!CS_LOG_QUIET_BOOL&&(_PREDICATE)){printf(__VA_ARGS__);printf("\n");}
#endif

#if defined(CRANKSHAFT_NOLOGS) || defined(CRANSHAFT_ERROR_LOGS_ONLY) || defined(CRANSHAFT_NO_VERBOSE_LOGS)
#define CS_LOG_VERBOSE(...) {}
#define CS_LOG_VERBOSE_IF(...)
#define CS_LOG_INFO(...) {}
#define CS_LOG_INFO_IF(...)
#else
#define CS_LOG_VERBOSE(...) if(CS_LOG_VERBOSE_BOOL){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_VERBOSE_IF(_PREDICATE,...) if(CS_LOG_VERBOSE_BOOL&&(_PREDICATE)){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_INFO(...) if(CS_LOG_INFO_BOOL){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_INFO_IF(_PREDICATE,...) if(CS_LOG_INFO_BOOL&&(_PREDICATE)){printf(__VA_ARGS__);printf("\n");}
#endif

#if defined(CRANKSHAFT_NOLOGS) || defined(CRANSHAFT_ERROR_LOGS_ONLY) || defined(CRANSHAFT_NO_VERBOSE_LOGS) || defined(CRANKSHAFT_NO_TRACE_LOGS)
#define CS_LOG_TRACE(...) {}
#define CS_LOG_TRACE_IF(...) {}
#else
#define CS_LOG_TRACE(...) if(CS_LOG_TRACE_BOOL){printf(__VA_ARGS__);printf("\n");}
#define CS_LOG_TRACE_IF(_PREDICATE,...) if(CS_LOG_TRACE_BOOL&&(_PREDICATE)){printf(__VA_ARGS__);printf("\n");}
#endif

#ifdef __cplusplus
}
#endif
#endif
