/*
 * test-library.cpp - Test & Benchmark utility for cameraraw.library
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <proto/exec.h>
#include <exec/libraries.h>
#include <exec/interfaces.h>
#include "../cameraraw.library/cameraraw.h"

/* Stack cookie to run with 4MB stack on AmigaOS 4 */
static const char __attribute__((used)) stack_cookie[] = "$STACK:4000000";

/* Mirroring processed image structure from LibRaw */
typedef struct
{
    int type;
    unsigned short height, width, colors, bits;
    unsigned int data_size;
    unsigned char data[1];
} libraw_processed_image_t;

static unsigned long get_elapsed_ms(struct timeval *start, struct timeval *end)
{
    long sec = end->tv_sec - start->tv_sec;
    long usec = end->tv_usec - start->tv_usec;
    if (usec < 0) {
        sec--;
        usec += 1000000;
    }
    return (unsigned long)(sec * 1000 + usec / 1000);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("========================================================\n");
        printf(" cameraraw.library Test & Benchmark Tool\n");
        printf("========================================================\n");
        printf("Usage: %s <rawfile> [library_name_or_path]\n\n", argv[0]);
        printf("Examples:\n");
        printf("  %s photo.nef\n", argv[0]);
        printf("  %s photo.nef LIBS:cameraraw.library\n", argv[0]);
        printf("  %s photo.nef cameraraw.library.a1222\n", argv[0]);
        return 1;
    }

    const char *rawfile = argv[1];
    const char *custom_lib = (argc >= 3) ? argv[2] : NULL;

    const char *candidate_paths[10];
    int num_paths = 0;
    if (custom_lib && custom_lib[0]) {
        candidate_paths[num_paths++] = custom_lib;
    }
    candidate_paths[num_paths++] = "LIBS:cameraraw.library";
    candidate_paths[num_paths++] = "PROGDIR:cameraraw.library.a1222";
    candidate_paths[num_paths++] = "cameraraw.library.a1222";
    candidate_paths[num_paths++] = "PROGDIR:cameraraw.library";
    candidate_paths[num_paths++] = "cameraraw.library";
    candidate_paths[num_paths] = NULL;

    struct Library *CameraRawBase = NULL;
    const char *loaded_path = NULL;

    for (int i = 0; candidate_paths[i] != NULL; i++) {
        CameraRawBase = IExec->OpenLibrary(candidate_paths[i], 1);
        if (!CameraRawBase) {
            CameraRawBase = IExec->OpenLibrary(candidate_paths[i], 0);
        }
        if (CameraRawBase) {
            loaded_path = candidate_paths[i];
            break;
        }
    }

    if (!CameraRawBase) {
        printf("ERROR: Could not open cameraraw.library!\n");
        printf("Tried searching:\n");
        for (int i = 0; candidate_paths[i] != NULL; i++) {
            printf("  - %s\n", candidate_paths[i]);
        }
        return 1;
    }

    printf("========================================================\n");
    printf(" Opened library: %s\n", loaded_path);
    printf(" Version: %u.%u (ID: %s)\n",
           CameraRawBase->lib_Version, CameraRawBase->lib_Revision,
           CameraRawBase->lib_IdString ? (const char *)CameraRawBase->lib_IdString : "Unknown");
    printf("========================================================\n");

    struct CameraRawIFace *ICameraRaw = (struct CameraRawIFace *)IExec->GetInterface(CameraRawBase, "main", 1, NULL);
    if (!ICameraRaw) {
        printf("ERROR: Could not get 'main' interface from %s!\n", loaded_path);
        IExec->CloseLibrary(CameraRawBase);
        return 1;
    }

    /* Initialize CameraRaw context */
    APTR handle = ICameraRaw->CameraRaw_Init(ICameraRaw, 0);
    if (!handle) {
        printf("ERROR: CameraRaw_Init failed!\n");
        IExec->DropInterface((struct Interface *)ICameraRaw);
        IExec->CloseLibrary(CameraRawBase);
        return 1;
    }

    struct timeval t_start, t_end, t_step_start, t_step_end;
    gettimeofday(&t_start, NULL);

    /* 1. Open File */
    printf("[1/4] Opening RAW file: %s ...\n", rawfile);
    gettimeofday(&t_step_start, NULL);
    int ret = ICameraRaw->CameraRaw_OpenFile(ICameraRaw, handle, rawfile);
    gettimeofday(&t_step_end, NULL);
    if (ret != 0) {
        printf("ERROR: CameraRaw_OpenFile failed with code %d: %s\n",
               ret, ICameraRaw->CameraRaw_StrError(ICameraRaw, ret));
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        IExec->DropInterface((struct Interface *)ICameraRaw);
        IExec->CloseLibrary(CameraRawBase);
        return 1;
    }
    printf("      OK (took %lu ms)\n", get_elapsed_ms(&t_step_start, &t_step_end));

    /* Display Metadata */
    const char *make = ICameraRaw->CameraRaw_GetMake(ICameraRaw, handle);
    const char *model = ICameraRaw->CameraRaw_GetModel(ICameraRaw, handle);
    const char *lens = ICameraRaw->CameraRaw_GetLens(ICameraRaw, handle);
    float aperture = ICameraRaw->CameraRaw_GetAperture(ICameraRaw, handle);
    float shutter = ICameraRaw->CameraRaw_GetShutter(ICameraRaw, handle);
    float iso = ICameraRaw->CameraRaw_GetISO(ICameraRaw, handle);

    printf("      Camera: %s %s\n", (make && make[0]) ? make : "Unknown", (model && model[0]) ? model : "");
    if (lens && lens[0]) printf("      Lens  : %s\n", lens);
    if (aperture > 0.0f || shutter > 0.0f || iso > 0.0f) {
        printf("      EXIF  : ");
        if (aperture > 0.0f) {
            unsigned int ap_int = (unsigned int)aperture;
            unsigned int ap_frac = (unsigned int)(aperture * 10.0f) % 10;
            printf("f/%u.%u, ", ap_int, ap_frac);
        }
        if (shutter > 0.0f && shutter < 1.0f) {
            unsigned int inv_shutter = (unsigned int)(1.0f / shutter + 0.5f);
            printf("1/%u s, ", inv_shutter);
        } else if (shutter >= 1.0f) {
            unsigned int sh_int = (unsigned int)shutter;
            unsigned int sh_frac = (unsigned int)(shutter * 10.0f) % 10;
            printf("%u.%u s, ", sh_int, sh_frac);
        }
        if (iso > 0.0f) {
            printf("ISO %u", (unsigned int)iso);
        }
        printf("\n");
    }

    /* 2. Unpack Raw Data */
    printf("[2/4] Unpacking RAW sensor data ...\n");
    gettimeofday(&t_step_start, NULL);
    ret = ICameraRaw->CameraRaw_Unpack(ICameraRaw, handle);
    gettimeofday(&t_step_end, NULL);
    if (ret != 0) {
        printf("ERROR: CameraRaw_Unpack failed with code %d: %s\n",
               ret, ICameraRaw->CameraRaw_StrError(ICameraRaw, ret));
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        IExec->DropInterface((struct Interface *)ICameraRaw);
        IExec->CloseLibrary(CameraRawBase);
        return 1;
    }
    printf("      OK (took %lu ms)\n", get_elapsed_ms(&t_step_start, &t_step_end));

    /* 3. Process Image (Demosaic, Color Matrix, White Balance) */
    printf("[3/4] Processing image (Demosaic / Matrix / WB) ...\n");
    gettimeofday(&t_step_start, NULL);
    ret = ICameraRaw->CameraRaw_Process(ICameraRaw, handle);
    gettimeofday(&t_step_end, NULL);
    if (ret != 0) {
        printf("ERROR: CameraRaw_Process failed with code %d: %s\n",
               ret, ICameraRaw->CameraRaw_StrError(ICameraRaw, ret));
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        IExec->DropInterface((struct Interface *)ICameraRaw);
        IExec->CloseLibrary(CameraRawBase);
        return 1;
    }
    unsigned long process_ms = get_elapsed_ms(&t_step_start, &t_step_end);
    printf("      OK (took %lu ms - %lu.%02lu s)\n", process_ms, process_ms / 1000, (process_ms % 1000) / 10);

    /* 4. Render Memory Image */
    printf("[4/4] Generating RGB memory image ...\n");
    gettimeofday(&t_step_start, NULL);
    int mem_error = 0;
    libraw_processed_image_t *img = (libraw_processed_image_t *)ICameraRaw->CameraRaw_MakeMemImage(ICameraRaw, handle, &mem_error);
    gettimeofday(&t_step_end, NULL);
    if (!img) {
        printf("ERROR: CameraRaw_MakeMemImage failed with code %d: %s\n",
               mem_error, ICameraRaw->CameraRaw_StrError(ICameraRaw, mem_error));
    } else {
        printf("      OK (took %lu ms)\n", get_elapsed_ms(&t_step_start, &t_step_end));
        printf("\nImage Details:\n");
        printf("  Width x Height: %d x %d pixels\n", img->width, img->height);
        printf("  Color Channels: %d\n", img->colors);
        printf("  Bits per Channel: %d-bit\n", img->bits);
        unsigned long mb_int = img->data_size / (1024 * 1024);
        unsigned long mb_frac = ((img->data_size % (1024 * 1024)) * 100) / (1024 * 1024);
        printf("  Buffer Size: %u bytes (%lu.%02lu MB)\n", img->data_size, mb_int, mb_frac);

        ICameraRaw->CameraRaw_FreeMemImage(ICameraRaw, (APTR)img);
    }

    gettimeofday(&t_end, NULL);
    unsigned long total_ms = get_elapsed_ms(&t_start, &t_end);

    printf("========================================================\n");
    printf(" TOTAL DECODING TIME: %lu.%02lu seconds (%lu ms)\n", total_ms / 1000, (total_ms % 1000) / 10, total_ms);
    printf("========================================================\n");

    /* Cleanup */
    ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
    IExec->DropInterface((struct Interface *)ICameraRaw);
    IExec->CloseLibrary(CameraRawBase);

    return 0;
}
