/*
 * raw-viewer.cpp
 *
 * A simple command-line RAW image viewer for AmigaOS 4.
 * Uses LibRaw to decode and display RAW images in an Intuition window.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/asl.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>

#include "../cameraraw.library/cameraraw.h"

/* Mirrored LibRaw processed image struct */
typedef struct
{
    int type;
    unsigned short height, width, colors, bits;
    unsigned int data_size;
    unsigned char data[1];
} libraw_processed_image_t;

#ifdef __amigaos4__
static const char __attribute__((used)) stack_cookie[] = "$STACK:2000000";
#endif

#ifndef RECTFMT_RGB
#define RECTFMT_RGB 0UL
#endif

struct Library *IntuitionBase = NULL;
struct IntuitionIFace *IIntuition = NULL;

struct Library *GfxBase = NULL;
struct GraphicsIFace *IGraphics = NULL;

struct Library *AslBase = NULL;
struct AslIFace *IAsl = NULL;

struct Library *CameraRawBase = NULL;
struct CameraRawIFace *ICameraRaw = NULL;

void CleanUpOS()
{
    if (ICameraRaw) IExec->DropInterface((struct Interface *)ICameraRaw);
    if (CameraRawBase) IExec->CloseLibrary(CameraRawBase);

    if (IAsl) IExec->DropInterface((struct Interface *)IAsl);
    if (AslBase) IExec->CloseLibrary(AslBase);

    if (IGraphics) IExec->DropInterface((struct Interface *)IGraphics);
    if (GfxBase) IExec->CloseLibrary(GfxBase);

    if (IIntuition) IExec->DropInterface((struct Interface *)IIntuition);
    if (IntuitionBase) IExec->CloseLibrary(IntuitionBase);
}

bool InitOS()
{
    IntuitionBase = IExec->OpenLibrary("intuition.library", 50);
    if (IntuitionBase) {
        IIntuition = (struct IntuitionIFace *)IExec->GetInterface(IntuitionBase, "main", 1, NULL);
    }

    GfxBase = IExec->OpenLibrary("graphics.library", 50);
    if (GfxBase) {
        IGraphics = (struct GraphicsIFace *)IExec->GetInterface(GfxBase, "main", 1, NULL);
    }

    AslBase = IExec->OpenLibrary("asl.library", 50);
    if (AslBase) {
        IAsl = (struct AslIFace *)IExec->GetInterface(AslBase, "main", 1, NULL);
    }

    CameraRawBase = IExec->OpenLibrary("cameraraw.library", 1);
    if (CameraRawBase) {
        ICameraRaw = (struct CameraRawIFace *)IExec->GetInterface(CameraRawBase, "main", 1, NULL);
    }

    if (!IIntuition || !IGraphics || !IAsl || !ICameraRaw) {
        printf("Failed to open required AmigaOS interfaces (including cameraraw.library).\n");
        CleanUpOS();
        return false;
    }
    return true;
}

int main(int ac, char *av[])
{
    if (!InitOS()) {
        return 20;
    }

    std::string filename;

    if (ac > 1) {
        filename = av[1];
    } else {
        // Open ASL File Requester
        struct FileRequester *req = (struct FileRequester *)IAsl->AllocAslRequestTags(ASL_FileRequest,
            ASLFR_TitleText, (ULONG)"Select RAW Image",
            ASLFR_RejectIcons, TRUE,
            TAG_DONE);

        if (req) {
            if (IAsl->AslRequestTags(req, TAG_DONE)) {
                std::string drawer = req->fr_Drawer;
                std::string file = req->fr_File;
                if (!drawer.empty() && drawer[drawer.length() - 1] != '/' && drawer[drawer.length() - 1] != ':') {
                    drawer += "/";
                }
                filename = drawer + file;
            }
            IAsl->FreeAslRequest(req);
        }
    }

    if (filename.empty()) {
        printf("No file selected.\n");
        CleanUpOS();
        return 0;
    }

    printf("Opening image: %s...\n", filename.c_str());

    APTR handle = ICameraRaw->CameraRaw_Init(ICameraRaw, CAMERARAW_INIT_HALF_SIZE);
    if (!handle) {
        printf("Failed to initialize CameraRaw handle.\n");
        CleanUpOS();
        return 10;
    }

    int ret = ICameraRaw->CameraRaw_OpenFile(ICameraRaw, handle, filename.c_str());
    if (ret != 0) {
        printf("Failed to open file, error code: %d\n", ret);
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        CleanUpOS();
        return 10;
    }

    printf("Unpacking RAW data...\n");
    if ((ret = ICameraRaw->CameraRaw_Unpack(ICameraRaw, handle)) != 0) {
        printf("Failed to unpack, error code: %d\n", ret);
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        CleanUpOS();
        return 10;
    }

    printf("Processing image...\n");
    if ((ret = ICameraRaw->CameraRaw_Process(ICameraRaw, handle)) != 0) {
        printf("Failed to process, error code: %d\n", ret);
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        CleanUpOS();
        return 10;
    }

    int err = 0;
    libraw_processed_image_t *img = (libraw_processed_image_t *)ICameraRaw->CameraRaw_MakeMemImage(ICameraRaw, handle, &err);
    if (!img) {
        printf("Failed to create memory image: %d\n", err);
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        CleanUpOS();
        return 10;
    }

    if (img->colors != 3) {
        printf("Unsupported processed image format (must be 3-color RGB).\n");
        ICameraRaw->CameraRaw_FreeMemImage(ICameraRaw, (APTR)img);
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        CleanUpOS();
        return 10;
    }

    printf("Decoded successfully: %d x %d pixels\n", img->width, img->height);

    // Get screen dimensions to limit window size
    ULONG scr_w = 800, scr_h = 600;
    struct Screen *scr = IIntuition->LockPubScreen(NULL);
    if (scr) {
        scr_w = scr->Width;
        scr_h = scr->Height;
        IIntuition->UnlockPubScreen(NULL, scr);
    }

    ULONG win_w = img->width;
    ULONG win_h = img->height;
    if (win_w > scr_w - 50) win_w = scr_w - 50;
    if (win_h > scr_h - 80) win_h = scr_h - 80;

    struct Window *win = IIntuition->OpenWindowTags(NULL,
        WA_Title, (ULONG)"LibRaw Image Viewer (AmigaOS 4)",
        WA_InnerWidth, win_w,
        WA_InnerHeight, win_h,
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
        TAG_DONE);

    if (!win) {
        printf("Failed to open Intuition window.\n");
        ICameraRaw->CameraRaw_FreeMemImage(ICameraRaw, (APTR)img);
        ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
        CleanUpOS();
        return 10;
    }

    // Draw the image immediately
    ULONG draw_w = win->Width - win->BorderLeft - win->BorderRight;
    ULONG draw_h = win->Height - win->BorderTop - win->BorderBottom;
    if (draw_w > (ULONG)img->width) draw_w = img->width;
    if (draw_h > (ULONG)img->height) draw_h = img->height;

    IGraphics->WritePixelArray(
        (uint8 *)img->data,
        0,
        0,
        img->width * 3,
        PIXF_R8G8B8,
        win->RPort,
        win->BorderLeft,
        win->BorderTop,
        draw_w,
        draw_h
    );

    printf("Window opened. Close the window to exit.\n");

    // Message Loop
    BOOL running = TRUE;
    while (running) {
        uint32 wait = IExec->Wait(1L << win->UserPort->mp_SigBit | SIGBREAKF_CTRL_C);

        if (wait & SIGBREAKF_CTRL_C) {
            running = FALSE;
            break;
        }

        struct IntuiMessage *msg;
        while ((msg = (struct IntuiMessage *)IExec->GetMsg(win->UserPort))) {
            uint32 msgClass = msg->Class;
            IExec->ReplyMsg((struct Message *)msg);

            switch (msgClass) {
                case IDCMP_CLOSEWINDOW:
                    running = FALSE;
                    break;

                case IDCMP_REFRESHWINDOW:
                    IIntuition->BeginRefresh(win);
                    draw_w = win->Width - win->BorderLeft - win->BorderRight;
                    draw_h = win->Height - win->BorderTop - win->BorderBottom;
                    if (draw_w > (ULONG)img->width) draw_w = img->width;
                    if (draw_h > (ULONG)img->height) draw_h = img->height;

                    IGraphics->WritePixelArray(
                        (uint8 *)img->data,
                        0,
                        0,
                        img->width * 3,
                        PIXF_R8G8B8,
                        win->RPort,
                        win->BorderLeft,
                        win->BorderTop,
                        draw_w,
                        draw_h
                    );
                    IIntuition->EndRefresh(win, TRUE);
                    break;
            }
        }
    }

    IIntuition->CloseWindow(win);
    ICameraRaw->CameraRaw_FreeMemImage(ICameraRaw, (APTR)img);
    ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
    CleanUpOS();

    printf("Exit viewer.\n");
    return 0;
}
