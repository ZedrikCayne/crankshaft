#ifndef __crankshafttempbuffdoth__
#define __crankshafttempbuffdoth__

#include <stdbool.h>

/****************************************
 *
 * Temp Buffer system.
 *
 * Sets a default number of temporary
 * buffers. (Sizes from 256 bytes to
 * 16384). Growing in powers of 2.
 *
 * Ask for a buffer of size, get a buffer.
 * No nulls unless it isn't initialized in
 * the first place or you ask for a buffer
 * bigger than the currently defined max.
 * CS_MAX_TEMP_BUFF_SIZE
 *
 * These buffers should not be trusted for
 * very long. (There are only 64 16k buffers
 * for example) and definitely not past
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

#define CS_MIN_TEMP_BUFF_SIZE 256
#define CS_MAX_TEMP_BUFF_SIZE 16384
#define CS_TEMP_BUFF_ALLOC_SIZE (1024 * 1024)
#define CS_TEMP_BUFF_ALIGNMENT 4

void *CS_tempBuff(int size);
bool CS_allocateTempBuffs(void);
bool CS_freeAllTempBuffs(void);

#define CS_MAX_TEMP_BUFF_TEMP_NAME 64

void *CS_allocManualTempBuff(const char *name, int elementSize, int numberOfElements, int alignment);
void *CS_getTempBuff(void *manualTempBuff);
void CS_freeManualTempBuff(void *manualTempBuff);

#ifdef __cplusplus
}
#endif
#endif
