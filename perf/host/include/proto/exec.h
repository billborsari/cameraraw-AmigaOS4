#ifndef HOST_PROTO_EXEC_H
#define HOST_PROTO_EXEC_H
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/interfaces.h>
struct ExecIFace {
  struct Library *(*OpenLibrary)(const char *name, uint32 version);
  void (*CloseLibrary)(struct Library *);
  struct Interface *(*GetInterface)(struct Library *, const char *name, uint32 version, void *tags);
  void (*DropInterface)(struct Interface *);
  void (*DebugPrintF)(const char *fmt, ...);
};
#ifdef __cplusplus
extern "C"
#endif
struct ExecIFace *IExec;
#endif
