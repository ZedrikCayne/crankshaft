#ifndef __crankshafttempbuffdoth__
#define __crankshafttempbuffdoth__

#include <stdbool.h>

/****************************************
 *
 * Temp Buffer system.
 *
 * Ask for a buffer of size, get a buffer.
 * No nulls unless it isn't initialized in
 * the first place or you ask for a buffer
 * bigger than the currently defined max.
 * CS_MAX_TEMP_BUFF_SIZE
 *
 * These buffers should not be trusted for
 * very long and definitely not past
 * the scope of an IO call of any kind or
 * even for the scope of an IO call if
 * the IO is slow. (Networked drives,
 * internet) Typically safe for console
 * printing. (Crankshaft relies upon them
 * heavily for logging)
 *
 * If you have a particular use case,
 * there is a provided 'ManualTempBuffer'
 * allocator so you can provide your own
 * or feel free to bump the numbers here
 * in either direction if you feel the
 * need. These are just reasonable defaults.
 *
 * Allocators are thread safe, as safe as
 * this kind of thing possibly can be
 * (Which means, not very)
 *
 ****************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define CS_TEMPBUFF_MIN_SIZE 8
#define CS_TEMPBUFF_MAX_SIZE 16384
#define CS_TEMPBUFF_ALIGNMENT 8

void *CS_tempBuff(int size);
char *CS_tempStringCopy(const char *copyMe);
bool CS_tempAllocateGlobal(int globalSize);
bool CS_tempFreeGlobal();
char *CS_tempBuffSnprintf(int max, char *fmt, ...);

#define CS_MAX_TEMP_BUFF_TEMP_NAME 64

void *CS_tempAllocManual(const char *name, int size );
void *CS_tempGetManual(void *manualTempBuff, int size);
void CS_tempFreeManual(void *manualTempBuff);

#ifdef __cplusplus
}
#endif
#endif
