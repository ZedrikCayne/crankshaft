#ifndef __crankshaftloggerdoth__
#define __crankshaftloggerdoth__
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/********************************************************************
 *
 * Basic engine logging support. Levels are as follows:
 *
 * ERROR,LOG,WARN,TRACE,INFO,VERBOSE. Each of which are toggle-able
 * (If for some reason you only want trace logs, but not errors,
 * go nuts) In main.cpp you are provided with an example of how you
 * can mix and match. (For example, we allow regular logs but do not
 * log errors)
 *
 * LOG_LOUD logs all the time except if you #define
 * CRANKSHAFT_ERROR_LOGS_ONLY which will squelch anything other than
 * an error during compile time.
 *
 * All logging has an _IF variant (CS_LOG_LOUD_IF) that takes a
 * boolean predicate. Be warned, it will not be evaluated if the
 * particular log type is turned off. So avoid structures like
 * CS_LOG_INFO( doesWork(), "Did work" ); because if you are not
 * logging info items, it will not call doesWork() (This should
 * be avoided in any case as all logging may be stripped)
 *
 * You can also #define CRANKSHAFT_NO_LOGS to completely quiet the
 * logging system except for explicit calls to
 * CS_log(__FILE__,__LINE__)
 *
 ********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

extern bool CS_LOG_ERROR_BOOL;
extern bool CS_LOG_QUIET_BOOL;
extern bool CS_LOG_WARN_BOOL;
extern bool CS_LOG_TRACE_BOOL;
extern bool CS_LOG_INFO_BOOL;
extern bool CS_LOG_VERBOSE_BOOL;
#if defined(CRANKSHAFT_NO_LOGS)
#define CS_LOG_STDERR(...) {}
#define CS_LOG_ERROR(...) {}
#define CS_LOG_ERROR_IF(...)
#else
#define CS_LOG_STDERR(...) if(CS_LOG_ERROR_BOOL){fprintf(stderr,__VA_ARGS__);}
#define CS_LOG_ERROR(...) if(CS_LOG_ERROR_BOOL){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_ERROR_IF(_PREDICATE,...) if(CS_LOG_ERROR_BOOL&&(_PREDICATE)){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#endif

#if defined(CRANKSHAFT_NO_LOGS) || defined(CRANKSHAFT_ERROR_LOGS_ONLY)
#define CS_LOG_LOUD(...) {}
#define CS_LOG_LOUD_IF(...)
#define CS_LOG_WARN(...) {}
#define CS_LOG_WARN_IF(...)
#define CS_LOG(...) {}
#define CS_LOG_IF(...)
#else
#define CS_LOG_LOUD(...) if(true){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_LOUD_IF(_PREDICATE,...) if(_PREDICATE){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_WARN(...) if(CS_LOG_WARN_BOOL){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_WARN_IF(_PREDICATE,...) if(CS_LOG_WARN_BOOL&&(_PREDICATE)){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG(...) if(!CS_LOG_QUIET_BOOL){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_IF(_PREDICATE,...) if(!CS_LOG_QUIET_BOOL&&(_PREDICATE)){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#endif

#if defined(CRANKSHAFT_NO_LOGS) || defined(CRANSHAFT_ERROR_LOGS_ONLY) || defined(CRANSHAFT_NO_VERBOSE_LOGS)
#define CS_LOG_VERBOSE(...) {}
#define CS_LOG_VERBOSE_IF(...)
#define CS_LOG_INFO(...) {}
#define CS_LOG_INFO_IF(...)
#else
#define CS_LOG_VERBOSE(...) if(CS_LOG_VERBOSE_BOOL){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_VERBOSE_IF(_PREDICATE,...) if(CS_LOG_VERBOSE_BOOL&&(_PREDICATE)){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_INFO(...) if(CS_LOG_INFO_BOOL){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_INFO_IF(_PREDICATE,...) if(CS_LOG_INFO_BOOL&&(_PREDICATE)){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#endif

#if defined(CRANKSHAFT_NO_LOGS) || defined(CRANSHAFT_ERROR_LOGS_ONLY) || defined(CRANSHAFT_NO_VERBOSE_LOGS) || defined(CRANKSHAFT_NO_TRACE_LOGS)
#define CS_LOG_TRACE(...) {}
#define CS_LOG_TRACE_IF(...) {}
#else
#define CS_LOG_TRACE(...) if(CS_LOG_TRACE_BOOL){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#define CS_LOG_TRACE_IF(_PREDICATE,...) if(CS_LOG_TRACE_BOOL&&(_PREDICATE)){CS_log(__FILE__,__LINE__,__VA_ARGS__);}
#endif

void CS_log(const char *file, int32_t line, const char *fmt, ... );
void CS_logRotate(int32_t maxHistory);
void CS_logFile(char *fileName);

bool CS_logInit( const char *fileName );
bool CS_logKill(void);

#ifdef __cplusplus
}
#endif
#endif
