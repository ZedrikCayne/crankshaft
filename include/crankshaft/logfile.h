#ifndef __crankshaftlogfiledoth__
#define __crankshaftlogfiledoth__
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_LogFile;

struct CS_LogFile *CS_logfileCreate(const char *filename, int32_t rotates, int32_t maxBytes, int32_t secondsPerRotate);
bool CS_logfileClose(struct CS_LogFile *logfile);
bool CS_logfileRotate(struct CS_LogFile *logfile);
bool CS_logfileDestroy(struct CS_LogFile *logfile);
bool CS_logfileFlush(struct CS_LogFile *logfile);
int32_t CS_logfilePrintf(struct CS_LogFile *logfile, const char *format, ...)__attribute__((format(printf, 2, 3)));;

#ifdef __cplusplus
}
#endif
#endif
