/* Host (macOS/Linux) stand-in for the AmigaOS 4 headers that the cameraraw
 * sources include, so the same dcraw_emu / test-library / cameraraw_api.cpp
 * build natively for reference output and profiling. Not part of the
 * Amiga build. */
#ifndef HOST_EXEC_TYPES_H
#define HOST_EXEC_TYPES_H
#include <stdint.h>
typedef void *APTR;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef intptr_t BPTR;
typedef char *STRPTR;
typedef const char *CONST_STRPTR;
#define TAG_END 0
#endif
