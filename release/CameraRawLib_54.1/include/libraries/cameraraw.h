#ifndef LIBRARIES_CAMERARAW_H
#define LIBRARIES_CAMERARAW_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/interfaces.h>
#include <dos/dos.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization flags */
#define CAMERARAW_INIT_HALF_SIZE 1

struct CameraRawBase
{
    struct Library libNode;
    BPTR segList;
    struct ExecIFace *IExec;
};

struct CameraRawIFace
{
    struct InterfaceData Data;

    uint32 (*Obtain)(struct CameraRawIFace *Self);
    uint32 (*Release)(struct CameraRawIFace *Self);
    void (*Expunge)(struct CameraRawIFace *Self);
    struct Interface * (*Clone)(struct CameraRawIFace *Self);

    APTR (*CameraRaw_Init)(struct CameraRawIFace *Self, uint32 flags);
    int (*CameraRaw_OpenFile)(struct CameraRawIFace *Self, APTR handle, const char *filename);
    int (*CameraRaw_Unpack)(struct CameraRawIFace *Self, APTR handle);
    int (*CameraRaw_Process)(struct CameraRawIFace *Self, APTR handle);
    APTR (*CameraRaw_MakeMemImage)(struct CameraRawIFace *Self, APTR handle, int *errorcode);
    void (*CameraRaw_FreeMemImage)(struct CameraRawIFace *Self, APTR memImage);
    void (*CameraRaw_Recycle)(struct CameraRawIFace *Self, APTR handle);
    void (*CameraRaw_Close)(struct CameraRawIFace *Self, APTR handle);

    const char * (*CameraRaw_StrError)(struct CameraRawIFace *Self, int errorcode);
    const char * (*CameraRaw_StrProgress)(struct CameraRawIFace *Self, int progresscode);
    void (*CameraRaw_SetProgressHandler)(struct CameraRawIFace *Self, APTR handle, APTR callback, APTR callback_data);
    int (*CameraRaw_OpenBuffer)(struct CameraRawIFace *Self, APTR handle, void *buffer, uint32 size);
    int (*CameraRaw_PpmTiffWriter)(struct CameraRawIFace *Self, APTR handle, const char *filename);

    int (*CameraRaw_UnpackThumb)(struct CameraRawIFace *Self, APTR handle);
    int (*CameraRaw_ThumbWriter)(struct CameraRawIFace *Self, APTR handle, const char *filename);

    const char * (*CameraRaw_GetMake)(struct CameraRawIFace *Self, APTR handle);
    const char * (*CameraRaw_GetModel)(struct CameraRawIFace *Self, APTR handle);
    void (*CameraRaw_ApplySettings)(struct CameraRawIFace *Self, APTR handle, const char *settings_str);

    float (*CameraRaw_GetAperture)(struct CameraRawIFace *Self, APTR handle);
    float (*CameraRaw_GetShutter)(struct CameraRawIFace *Self, APTR handle);
    float (*CameraRaw_GetISO)(struct CameraRawIFace *Self, APTR handle);
    float (*CameraRaw_GetFocalLength)(struct CameraRawIFace *Self, APTR handle);
    const char * (*CameraRaw_GetLens)(struct CameraRawIFace *Self, APTR handle);
    uint32 (*CameraRaw_GetTimestamp)(struct CameraRawIFace *Self, APTR handle);
};

#ifdef __cplusplus
}
#endif

#endif /* LIBRARIES_CAMERARAW_H */
