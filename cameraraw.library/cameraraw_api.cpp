/*
 * cameraraw_api.cpp - Exported API functions of cameraraw.library
 * Wraps the C++ LibRaw class in a stack-safe, heap-allocated wrapper.
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
#include "cameraraw.h"
#include "libraw/libraw.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern struct ExecIFace *IExec;

extern "C" {

APTR _CameraRaw_CameraRaw_Init(struct CameraRawIFace *Self, uint32 flags)
{
    // Stack-safe dynamic allocation on the heap
    try {
        LibRaw *raw = new LibRaw(flags & ~CAMERARAW_INIT_HALF_SIZE);
        if (flags & CAMERARAW_INIT_HALF_SIZE) {
            raw->imgdata.params.half_size = 1;
        }
        return (APTR)raw;
    } catch (...) {
        return NULL;
    }
}

int _CameraRaw_CameraRaw_OpenFile(struct CameraRawIFace *Self, APTR handle, const char *filename)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw || !filename) return LIBRAW_IO_ERROR;

    int ret = raw->open_file(filename);
    if (ret != 0) {
        IExec->DebugPrintF("[cameraraw.library] LibRaw::open_file('%s') failed: %d\n", filename, ret);
    }
    return ret;
}

#include <exec/tasks.h>

static int RunOnLargeStack(int (*func)(LibRaw *), LibRaw *raw)
{
    uint32 stack_size = 1024 * 1024; // 1MB stack
    APTR new_stack = IExec->AllocVecTags(stack_size,
                                         AVT_Type, MEMF_SHARED,
                                         AVT_ClearWithValue, 0,
                                         TAG_END);
    if (!new_stack) {
        return func(raw);
    }

    struct StackSwapStruct stk;
    stk.stk_Lower = new_stack;
    stk.stk_Upper = (uint32)new_stack + stack_size;
    stk.stk_Pointer = (APTR)(((uint32)new_stack + stack_size - 32) & ~15);

    int res;
    void *stackswap_fn = (void *)IExec->StackSwap;

    __asm__ volatile (
        "mr 31, %[stk]\n\t"
        "mr 27, %[IExec]\n\t"
        "mr 29, %[func]\n\t"
        "mr 28, %[raw]\n\t"
        
        "mr 3, 27\n\t"
        "mr 4, 31\n\t"
        "mtctr %[stackswap]\n\t"
        "bctrl\n\t"
        
        "mr 3, 28\n\t"
        "mtctr 29\n\t"
        "bctrl\n\t"
        "mr %[res], 3\n\t"
        
        "mr 3, 27\n\t"
        "mr 4, 31\n\t"
        "mtctr %[stackswap]\n\t"
        "bctrl\n\t"
        : [res] "=r" (res)
        : [stk] "r" (&stk), [IExec] "r" (IExec), [func] "r" (func), [raw] "r" (raw), [stackswap] "r" (stackswap_fn)
        : "r0", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
          "r27", "r28", "r29", "r31", "lr", "ctr", "memory"
    );

    IExec->FreeVec(new_stack);
    return res;
}

static int do_unpack(LibRaw *raw)
{
    return raw->unpack();
}

static int do_process(LibRaw *raw)
{
    return raw->dcraw_process();
}

int _CameraRaw_CameraRaw_Unpack(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return LIBRAW_OUT_OF_ORDER_CALL;
    return RunOnLargeStack(do_unpack, raw);
}

int _CameraRaw_CameraRaw_Process(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return LIBRAW_OUT_OF_ORDER_CALL;
    return RunOnLargeStack(do_process, raw);
}

APTR _CameraRaw_CameraRaw_MakeMemImage(struct CameraRawIFace *Self, APTR handle, int *errorcode)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) {
        if (errorcode) *errorcode = LIBRAW_OUT_OF_ORDER_CALL;
        return NULL;
    }
    return (APTR)raw->dcraw_make_mem_image(errorcode);
}

void _CameraRaw_CameraRaw_FreeMemImage(struct CameraRawIFace *Self, APTR memImage)
{
    if (memImage) {
        LibRaw::dcraw_clear_mem((libraw_processed_image_t *)memImage);
    }
}

void _CameraRaw_CameraRaw_Recycle(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (raw) {
        raw->recycle();
    }
}

void _CameraRaw_CameraRaw_Close(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (raw) {
        delete raw;
    }
}

const char * _CameraRaw_CameraRaw_StrError(struct CameraRawIFace *Self, int errorcode)
{
    return libraw_strerror(errorcode);
}

const char * _CameraRaw_CameraRaw_StrProgress(struct CameraRawIFace *Self, int progresscode)
{
    return libraw_strprogress((enum LibRaw_progress)progresscode);
}

void _CameraRaw_CameraRaw_SetProgressHandler(struct CameraRawIFace *Self, APTR handle, APTR callback, APTR callback_data)
{
    LibRaw *raw = (LibRaw *)handle;
    if (raw) {
        raw->set_progress_handler((progress_callback)callback, callback_data);
    }
}

int _CameraRaw_CameraRaw_OpenBuffer(struct CameraRawIFace *Self, APTR handle, void *buffer, uint32 size)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return LIBRAW_OUT_OF_ORDER_CALL;
    int ret = raw->open_buffer(buffer, size);
    return ret;
}

int _CameraRaw_CameraRaw_PpmTiffWriter(struct CameraRawIFace *Self, APTR handle, const char *filename)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw || !filename) return LIBRAW_OUT_OF_ORDER_CALL;
    return raw->dcraw_ppm_tiff_writer(filename);
}

int _CameraRaw_CameraRaw_UnpackThumb(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return LIBRAW_OUT_OF_ORDER_CALL;
    return raw->unpack_thumb();
}

int _CameraRaw_CameraRaw_ThumbWriter(struct CameraRawIFace *Self, APTR handle, const char *filename)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw || !filename) return LIBRAW_OUT_OF_ORDER_CALL;
    return raw->dcraw_thumb_writer(filename);
}

const char * _CameraRaw_CameraRaw_GetMake(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return "";
    return raw->imgdata.idata.make;
}

const char * _CameraRaw_CameraRaw_GetModel(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return "";
    return raw->imgdata.idata.model;
}

void _CameraRaw_CameraRaw_ApplySettings(struct CameraRawIFace *Self, APTR handle, const char *settings_str)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw || !settings_str) return;

    char *buf = strdup(settings_str);
    if (!buf) return;

    char *pair = strtok(buf, ";");
    while (pair != NULL) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            const char *key = pair;
            const char *val = eq + 1;

            if (strcmp(key, "scale") == 0) {
                if (strcmp(val, "half") == 0 || strcmp(val, "3x") == 0 || strcmp(val, "4x") == 0 || strcmp(val, "screen") == 0) {
                    raw->imgdata.params.half_size = 1;
                } else {
                    raw->imgdata.params.half_size = 0;
                }
            } else if (strcmp(key, "auto_bright") == 0) {
                int ab = atoi(val);
                raw->imgdata.params.no_auto_bright = (ab == 0) ? 1 : 0;
            } else if (strcmp(key, "bright") == 0) {
                raw->imgdata.params.bright = (float)atof(val);
            } else if (strcmp(key, "wb") == 0) {
                if (strcmp(val, "camera") == 0) {
                    raw->imgdata.params.use_camera_wb = 1;
                    raw->imgdata.params.use_auto_wb = 0;
                } else if (strcmp(val, "auto") == 0) {
                    raw->imgdata.params.use_camera_wb = 0;
                    raw->imgdata.params.use_auto_wb = 1;
                } else {
                    raw->imgdata.params.use_camera_wb = 0;
                    raw->imgdata.params.use_auto_wb = 0;
                }
            } else if (strcmp(key, "exp_correc") == 0) {
                raw->imgdata.params.exp_correc = atoi(val);
            } else if (strcmp(key, "exp_shift") == 0) {
                raw->imgdata.params.exp_shift = (float)atof(val);
            } else if (strcmp(key, "threshold") == 0) {
                raw->imgdata.params.threshold = (float)atof(val);
            }
        }
        pair = strtok(NULL, ";");
    }
    free(buf);
}

float _CameraRaw_CameraRaw_GetAperture(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return 0.0f;
    return raw->imgdata.other.aperture;
}

float _CameraRaw_CameraRaw_GetShutter(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return 0.0f;
    return raw->imgdata.other.shutter;
}

float _CameraRaw_CameraRaw_GetISO(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return 0.0f;
    return raw->imgdata.other.iso_speed;
}

float _CameraRaw_CameraRaw_GetFocalLength(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return 0.0f;
    return raw->imgdata.other.focal_len;
}

const char * _CameraRaw_CameraRaw_GetLens(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return "";
    return raw->imgdata.lens.Lens;
}

uint32 _CameraRaw_CameraRaw_GetTimestamp(struct CameraRawIFace *Self, APTR handle)
{
    LibRaw *raw = (LibRaw *)handle;
    if (!raw) return 0;
    return (uint32)raw->imgdata.other.timestamp;
}

} // extern "C"
