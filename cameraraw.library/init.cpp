/*
 * init.cpp - AmigaOS 4 Shared Library Initialization Boilerplate
 *
 * Copyright (C) 2026 Bill Borsari
 * Based on LibRaw (C) 2008-2025 LibRaw LLC (http://www.libraw.org)
 *
 * This file is part of cameraraw.library.
 *
 * cameraraw.library is free software; you can redistribute it and/or modify
 * it under the terms of either:
 * 1. The GNU Lesser General Public License version 2.1 (LGPL 2.1); or
 * 2. The Common Development and Distribution License (CDDL) Version 1.0.
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR CDDL-1.0
 */

#include <proto/exec.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/interfaces.h>
#include "cameraraw.h"

/* Global Exec Interface pointer */
struct ExecIFace *IExec = NULL;

extern "C" {
struct Library *NewlibBase = NULL;
struct Interface *INewlib = NULL;
}

/* Name and Version of Library */
static const char libName[] = "cameraraw.library";
static const char libVer[] = "$VER: cameraraw.library 54.2 (" __DATE__ " " __TIME__ ")";

/* Forward Declarations */
extern "C" {
static BPTR libExpunge(struct LibraryManagerInterface *Self);
static struct Library * libClose(struct LibraryManagerInterface *Self);
static struct Library * libOpen(struct LibraryManagerInterface *Self, ULONG version);
static struct Library * libInit(struct Library *base, APTR segList, struct Interface *exec);

uint32 _CameraRaw_Obtain(struct CameraRawIFace *Self);
uint32 _CameraRaw_Release(struct CameraRawIFace *Self);
struct Interface * _CameraRaw_Clone(struct CameraRawIFace *Self);

/* Vector stub pointers from cameraraw_api.cpp */
extern APTR _CameraRaw_CameraRaw_Init(struct CameraRawIFace *Self, uint32 flags);
extern int _CameraRaw_CameraRaw_OpenFile(struct CameraRawIFace *Self, APTR handle, const char *filename);
extern int _CameraRaw_CameraRaw_Unpack(struct CameraRawIFace *Self, APTR handle);
extern int _CameraRaw_CameraRaw_Process(struct CameraRawIFace *Self, APTR handle);
extern APTR _CameraRaw_CameraRaw_MakeMemImage(struct CameraRawIFace *Self, APTR handle, int *errorcode);
extern void _CameraRaw_CameraRaw_FreeMemImage(struct CameraRawIFace *Self, APTR memImage);
extern void _CameraRaw_CameraRaw_Recycle(struct CameraRawIFace *Self, APTR handle);
extern void _CameraRaw_CameraRaw_Close(struct CameraRawIFace *Self, APTR handle);

extern const char * _CameraRaw_CameraRaw_StrError(struct CameraRawIFace *Self, int errorcode);
extern const char * _CameraRaw_CameraRaw_StrProgress(struct CameraRawIFace *Self, int progresscode);
extern void _CameraRaw_CameraRaw_SetProgressHandler(struct CameraRawIFace *Self, APTR handle, APTR callback, APTR callback_data);
extern int _CameraRaw_CameraRaw_OpenBuffer(struct CameraRawIFace *Self, APTR handle, void *buffer, uint32 size);
extern int _CameraRaw_CameraRaw_PpmTiffWriter(struct CameraRawIFace *Self, APTR handle, const char *filename);

extern int _CameraRaw_CameraRaw_UnpackThumb(struct CameraRawIFace *Self, APTR handle);
extern int _CameraRaw_CameraRaw_ThumbWriter(struct CameraRawIFace *Self, APTR handle, const char *filename);
extern const char * _CameraRaw_CameraRaw_GetMake(struct CameraRawIFace *Self, APTR handle);
extern const char * _CameraRaw_CameraRaw_GetModel(struct CameraRawIFace *Self, APTR handle);
extern void _CameraRaw_CameraRaw_ApplySettings(struct CameraRawIFace *Self, APTR handle, const char *settings_str);

extern float _CameraRaw_CameraRaw_GetAperture(struct CameraRawIFace *Self, APTR handle);
extern float _CameraRaw_CameraRaw_GetShutter(struct CameraRawIFace *Self, APTR handle);
extern float _CameraRaw_CameraRaw_GetISO(struct CameraRawIFace *Self, APTR handle);
extern float _CameraRaw_CameraRaw_GetFocalLength(struct CameraRawIFace *Self, APTR handle);
extern const char * _CameraRaw_CameraRaw_GetLens(struct CameraRawIFace *Self, APTR handle);
extern uint32 _CameraRaw_CameraRaw_GetTimestamp(struct CameraRawIFace *Self, APTR handle);
}

/* Main Interface Table */
static const APTR main_vectors[] = {
    (APTR)_CameraRaw_Obtain,
    (APTR)_CameraRaw_Release,
    NULL, /* Expunge - unimplemented */
    (APTR)_CameraRaw_Clone,
    (APTR)_CameraRaw_CameraRaw_Init,
    (APTR)_CameraRaw_CameraRaw_OpenFile,
    (APTR)_CameraRaw_CameraRaw_Unpack,
    (APTR)_CameraRaw_CameraRaw_Process,
    (APTR)_CameraRaw_CameraRaw_MakeMemImage,
    (APTR)_CameraRaw_CameraRaw_FreeMemImage,
    (APTR)_CameraRaw_CameraRaw_Recycle,
    (APTR)_CameraRaw_CameraRaw_Close,
    (APTR)_CameraRaw_CameraRaw_StrError,
    (APTR)_CameraRaw_CameraRaw_StrProgress,
    (APTR)_CameraRaw_CameraRaw_SetProgressHandler,
    (APTR)_CameraRaw_CameraRaw_OpenBuffer,
    (APTR)_CameraRaw_CameraRaw_PpmTiffWriter,
    (APTR)_CameraRaw_CameraRaw_UnpackThumb,
    (APTR)_CameraRaw_CameraRaw_ThumbWriter,
    (APTR)_CameraRaw_CameraRaw_GetMake,
    (APTR)_CameraRaw_CameraRaw_GetModel,
    (APTR)_CameraRaw_CameraRaw_ApplySettings,
    (APTR)_CameraRaw_CameraRaw_GetAperture,
    (APTR)_CameraRaw_CameraRaw_GetShutter,
    (APTR)_CameraRaw_CameraRaw_GetISO,
    (APTR)_CameraRaw_CameraRaw_GetFocalLength,
    (APTR)_CameraRaw_CameraRaw_GetLens,
    (APTR)_CameraRaw_CameraRaw_GetTimestamp,
    (APTR)-1
};

static const struct TagItem main_tags[] = {
    { MIT_Name, (ULONG)"main" },
    { MIT_VectorTable, (ULONG)main_vectors },
    { MIT_Version, 1 },
    { TAG_DONE }
};

/* Library Manager Table */
static const APTR manager_vectors[] = {
    (APTR)_CameraRaw_Obtain,
    (APTR)_CameraRaw_Release,
    NULL, /* Expunge */
    NULL, /* Clone */
    (APTR)libOpen,
    (APTR)libClose,
    (APTR)libExpunge,
    (APTR)-1
};

static const struct TagItem manager_tags[] = {
    { MIT_Name, (ULONG)"__library" },
    { MIT_VectorTable, (ULONG)manager_vectors },
    { MIT_Version, 1 },
    { TAG_DONE }
};

static const APTR interfaces[] = {
    (APTR)manager_tags,
    (APTR)main_tags,
    NULL
};

/* Library declaration table */
static const struct TagItem lib_tags[] = {
    { CLT_DataSize, sizeof(struct CameraRawBase) },
    { CLT_InitFunc, (ULONG)libInit },
    { CLT_Interfaces, (ULONG)interfaces },
    { TAG_DONE }
};

/* Resident structure */
extern "C" {
const struct Resident ROMTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&ROMTag,
    (struct Resident *)(&ROMTag + 1),
    RTF_AUTOINIT | RTF_NATIVE,
    1,
    NT_LIBRARY,
    0,
    (char *)libName,
    (char *)libVer + 6,
    (APTR)lib_tags
};
}

/* Entry point in case of running the library directly */
extern "C" int _start(void)
{
    return 1;
}

/* Global CTOR/DTOR lists defined by linker script */
extern "C" {
extern void (*__CTOR_LIST__[])(void);
extern void (*__CTOR_LIST_END__[])(void);
extern void (*__DTOR_LIST__[])(void);
extern void (*__DTOR_LIST_END__[])(void);
}

static void run_ctors(void)
{
    int num_ctors = (int)(__CTOR_LIST_END__ - __CTOR_LIST__);
    // IExec->DebugPrintF("[cameraraw.library] Running %ld global constructors (list start = %p, end = %p)...\n",
    //                    (long)num_ctors, __CTOR_LIST__, __CTOR_LIST_END__);
    for (int i = 0; i < num_ctors; i++) {
        if (__CTOR_LIST__[i]) {
            // IExec->DebugPrintF("[cameraraw.library] Calling constructor %ld: %p...\n", (long)i, __CTOR_LIST__[i]);
            __CTOR_LIST__[i]();
        }
    }
    // IExec->DebugPrintF("[cameraraw.library] Finished running global constructors.\n");
}

static void run_dtors(void)
{
    int num_dtors = (int)(__DTOR_LIST_END__ - __DTOR_LIST__);
    // IExec->DebugPrintF("[cameraraw.library] Running %ld global destructors...\n", (long)num_dtors);
    for (int i = num_dtors - 1; i >= 0; i--) {
        if (__DTOR_LIST__[i]) {
            // IExec->DebugPrintF("[cameraraw.library] Calling destructor %ld: %p...\n", (long)i, __DTOR_LIST__[i]);
            __DTOR_LIST__[i]();
        }
    }
    // IExec->DebugPrintF("[cameraraw.library] Finished running global destructors.\n");
}

/* Implementation of Standard Interface Functions */
extern "C" {
uint32 _CameraRaw_Obtain(struct CameraRawIFace *Self)
{
    return Self->Data.RefCount++;
}

uint32 _CameraRaw_Release(struct CameraRawIFace *Self)
{
    return --Self->Data.RefCount;
}

struct Interface * _CameraRaw_Clone(struct CameraRawIFace *Self)
{
    return NULL;
}
}

/* Implementation of Library Manager Functions */
extern "C" {
static struct Library * libInit(struct Library *base, APTR segList, struct Interface *exec)
{
    struct CameraRawBase *crBase = (struct CameraRawBase *)base;
    IExec = (struct ExecIFace *)exec;

    /* Open newlib.library for standard memory/C runtime support */
    NewlibBase = IExec->OpenLibrary("newlib.library", 4);
    if (!NewlibBase) {
        return NULL;
    }
    INewlib = IExec->GetInterface(NewlibBase, "main", 1, NULL);
    if (!INewlib) {
        IExec->CloseLibrary(NewlibBase);
        NewlibBase = NULL;
        return NULL;
    }

    crBase->libNode.lib_Node.ln_Type = NT_LIBRARY;
    crBase->libNode.lib_Node.ln_Pri  = 0;
    crBase->libNode.lib_Node.ln_Name = (char *)libName;
    crBase->libNode.lib_Flags        = LIBF_CHANGED | LIBF_SUMUSED;
    crBase->libNode.lib_Version      = 54;
    crBase->libNode.lib_Revision     = 2;
    crBase->libNode.lib_IdString     = (char *)libVer + 6;

    crBase->segList = (BPTR)segList;
    crBase->IExec = IExec;

    /* Call global constructors */
    run_ctors();

    return base;
}

static struct Library * libOpen(struct LibraryManagerInterface *Self, ULONG version)
{
    struct CameraRawBase *base = (struct CameraRawBase *)Self->Data.LibBase;
    base->libNode.lib_OpenCnt++;
    base->libNode.lib_Flags &= ~LIBF_DELEXP;
    return (struct Library *)base;
}

static struct Library * libClose(struct LibraryManagerInterface *Self)
{
    struct CameraRawBase *base = (struct CameraRawBase *)Self->Data.LibBase;
    if (base->libNode.lib_OpenCnt > 0) {
        base->libNode.lib_OpenCnt--;
    }
    return NULL;
}

static BPTR libExpunge(struct LibraryManagerInterface *Self)
{
    struct CameraRawBase *base = (struct CameraRawBase *)Self->Data.LibBase;
    if (base->libNode.lib_OpenCnt == 0) {
        BPTR segList = base->segList;

        /* Call global destructors */
        run_dtors();

        if (INewlib) {
            IExec->DropInterface(INewlib);
            INewlib = NULL;
        }
        if (NewlibBase) {
            IExec->CloseLibrary(NewlibBase);
            NewlibBase = NULL;
        }

        IExec->Remove((struct Node *)base);
        IExec->DeleteLibrary((struct Library *)base);
        return segList;
    }
    base->libNode.lib_Flags |= LIBF_DELEXP;
    return 0;
}
}
