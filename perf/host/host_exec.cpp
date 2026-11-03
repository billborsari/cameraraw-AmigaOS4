/* Host stand-in for exec.library: "opens" cameraraw.library by handing out an
 * interface whose vectors point straight at cameraraw_api.cpp, so dcraw_emu and
 * test-library run the same code path natively as on AmigaOS 4. */
#include <stdarg.h>
#include <stdio.h>
#include <proto/exec.h>
#include "../../cameraraw.library/cameraraw.h"

extern "C" {
#define F(n) extern __typeof__(*((struct CameraRawIFace *)0)->CameraRaw_##n) _CameraRaw_CameraRaw_##n;
F(Init) F(OpenFile) F(Unpack) F(Process) F(MakeMemImage) F(FreeMemImage) F(Recycle) F(Close)
F(StrError) F(StrProgress) F(SetProgressHandler) F(OpenBuffer) F(PpmTiffWriter) F(UnpackThumb)
F(ThumbWriter) F(GetMake) F(GetModel) F(ApplySettings) F(GetAperture) F(GetShutter) F(GetISO)
F(GetFocalLength) F(GetLens) F(GetTimestamp)
#undef F
}

static struct Library lib = {54, 2, "cameraraw.library (host shim)"};
static struct CameraRawIFace iface;

static struct Library *h_open(const char *, uint32) { return &lib; }
static void h_close(struct Library *) {}
static struct Interface *h_getif(struct Library *, const char *, uint32, void *)
{
#define S(n) iface.CameraRaw_##n = _CameraRaw_CameraRaw_##n;
  S(Init) S(OpenFile) S(Unpack) S(Process) S(MakeMemImage) S(FreeMemImage) S(Recycle) S(Close)
  S(StrError) S(StrProgress) S(SetProgressHandler) S(OpenBuffer) S(PpmTiffWriter) S(UnpackThumb)
  S(ThumbWriter) S(GetMake) S(GetModel) S(ApplySettings) S(GetAperture) S(GetShutter) S(GetISO)
  S(GetFocalLength) S(GetLens) S(GetTimestamp)
#undef S
  return (struct Interface *)&iface;
}
static void h_drop(struct Interface *) {}
static void h_dbg(const char *fmt, ...)
{
  va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
static struct ExecIFace exec_iface = {h_open, h_close, h_getif, h_drop, h_dbg};
struct ExecIFace *IExec = &exec_iface;
