#ifndef __crankshaftlogfiledoth__
#define __crankshaftlogfiledoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CS_LogFile;

struct CS_LogFile *CS_logfileCreate(const char *filename, int rotates, int maxBytes, int secondsPerRotate);
bool CS_logfileClose(struct CS_LogFile *logfile);
bool CS_logfileRotate(struct CS_LogFile *logfile);
bool CS_logfileDestroy(struct CS_LogFile *logfile);
bool CS_logfileFlush(struct CS_LogFile *logfile);
int CS_logfilePrintf(struct CS_LogFile *logfile, const char *format, ...);

#ifdef __cplusplus
}
#endif
#endif
