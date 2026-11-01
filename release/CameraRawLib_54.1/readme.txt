cameraraw.library 54.1 (AmigaOS 4 Port)
=======================================

cameraraw.library is a high-performance shared library port of LibRaw 0.22.1
for AmigaOS 4.x. It exposes an Amiga-native shared library interface to handle
reading, unpacking, demosaicing, color-matrix transforms, and metadata query
operations for over 1,100 digital camera RAW formats.

This standalone release includes the shared library along with command-line
dcraw tools, documentation, developer headers, and native hardware optimization
for both standard PowerPC (classic FPU) and AmigaOne A1222 (Tabor e500v2 SPE).


Table of Contents
-----------------
1. Version Information & Hardware Support
2. Package Contents & Layout
3. Command-Line Tools
4. Installation
5. Developer Guide
6. License & Compliance Info


1. Version Information & Hardware Support
-----------------------------------------
- cameraraw.library version: 54.1 ($VER: cameraraw.library 54.1 (29.10.2026))
- Upstream LibRaw version: 0.22.1 Release (Alex Tutubalin <lexa@lexa.ru>)
- Build Target: PowerPC (ppc-amigaos-), newlib runtime, single-thread

Dual Architecture Builds Included:
- Standard PowerPC:
  * Compiled with standard PowerPC hardware FPU instructions.
  * Optimized for AmigaOne X5000, X1000, Sam460ex, Pegasos II, and classic G3/G4.
- AmigaOne A1222 (Tabor e500v2 SPE):
  * Compiled with GCC targeting the e500v2 core (-mcpu=8548 -Wa,-me500 -O3).
  * Generates 100% native Signal Processing Engine (SPE) paired SIMD instructions.
  * Completely bypasses kernel illegal instruction trap-and-emulation, achieving
    full native execution speed on the A1222.


2. Package Contents & Layout
----------------------------
CameraRawLib_54.1/
  ├── libs/
  │    └── cameraraw.library          - Standard PowerPC shared library
  ├── A1222/
  │    ├── libs/
  │    │    └── cameraraw.library     - AmigaOne A1222 SPE hardware-optimized library
  │    └── C/
  │         ├── dcraw_emu             - A1222 SPE dcraw emulator
  │         ├── raw-identify          - A1222 SPE metadata inspector
  │         ├── simple_dcraw          - A1222 SPE simple raw extractor
  │         └── test-library          - A1222 SPE test & benchmark utility
  ├── C/
  │    ├── dcraw_emu                  - Standard dcraw emulator
  │    ├── raw-identify               - Standard metadata inspector
  │    ├── simple_dcraw               - Standard simple raw extractor
  │    └── test-library               - Standard test & benchmark utility
  ├── include/
  │    ├── libraries/
  │    │    └── cameraraw.h           - C/C++ developer header file
  │    └── xml/
  │         └── cameraraw.xml         - AmigaOS 4 Interface definition file
  ├── Documentation/
  │    └── libs/
  │         └── cameraraw.library/
  │              ├── cameraraw.doc    - API reference guide in Amiga AutoDoc format
  │              ├── COPYRIGHT.txt    - LibRaw copyright file
  │              ├── LICENSE.LGPL.txt - GNU LGPL 2.1 license text
  │              └── LICENSE.CDDL.txt - CDDL 1.0 license text
  ├── Tools/
  │    └── detect_a1222               - Hardware architecture detection tool
  ├── dcraw_emu.txt                   - CLI usage guide for dcraw_emu
  ├── Install cameraraw.library       - Python AmigaOS 4 Installation Script
  └── readme.txt                      - This documentation file


3. Command-Line Tools
---------------------
- dcraw_emu:
  Full-featured command-line utility replicating the classic dcraw toolset.
  Supports exposure correction, white balance adjustment, custom demosaicing
  algorithms (AHD, PPG, DCB, VCD, LMMSE, etc.), half-size rendering, and
  export to PPM, TIFF, or JPEG.
  See dcraw_emu.txt for detailed flags and options.

- raw-identify:
  Inspects and prints detailed RAW camera metadata: camera make/model, ISO,
  shutter speed, aperture, lens profile, EXIF timestamp, image dimensions,
  and color filter array (CFA) pattern.

- simple_dcraw:
  Lightweight extraction utility for unpacking and saving raw sensor images.

- test-library:
  Direct verification and benchmark tool for cameraraw.library. Measures each
  decoding stage (OpenFile, Unpack, Process, MakeMemImage) with millisecond
  precision and prints EXIF and image statistics.
  Usage: test-library <rawfile> [path_to_library]


4. Installation
---------------
Automatic Installation:
Double-click the "Install cameraraw.library" icon. The modern AmigaOS 4 installer
will:
1. Automatically detect if running on an AmigaOne A1222 (Tabor) or standard PowerPC.
2. Install cameraraw.library to LIBS: (or your chosen system volume).
3. Install the dcraw CLI tools with a default to C: (allowing you to override to
   any preferred destination such as Work:Tools).
4. Optionally install SDK headers to SDK: and documentation to HELP:.
5. Validate all installed files using MD5 checksums.

Manual Installation:
- For AmigaOne A1222:
    Copy A1222/libs/cameraraw.library TO LIBS:
    Copy A1222/C/#? TO C:
- For standard PowerPC (X5000, X1000, Sam460, etc.):
    Copy libs/cameraraw.library TO LIBS:
    Copy C/#? TO C:
- Developer files (optional):
    Copy include/libraries/#? TO SDK:include/include_h/libraries/
    Copy include/xml/#? TO SDK:xml/


5. Developer Guide
------------------
To use cameraraw.library in your own applications:
1. Open the library:
   struct Library *CameraRawBase = IExec->OpenLibrary("cameraraw.library", 54);
2. Obtain the main interface:
   struct CameraRawIFace *ICameraRaw = (struct CameraRawIFace *)IExec->GetInterface(CameraRawBase, "main", 1, NULL);
3. Initialize a handle:
   APTR handle = ICameraRaw->CameraRaw_Init(ICameraRaw, 0);
4. Open the image file:
   int ret = ICameraRaw->CameraRaw_OpenFile(ICameraRaw, handle, "image.cr2");
5. Unpack raw data:
   ret = ICameraRaw->CameraRaw_Unpack(ICameraRaw, handle);
6. Process the image (demosaic and color transform):
   ret = ICameraRaw->CameraRaw_Process(ICameraRaw, handle);
7. Retrieve 24-bit/48-bit RGB memory bitmap:
   int err = 0;
   libraw_processed_image_t *img = (libraw_processed_image_t *)ICameraRaw->CameraRaw_MakeMemImage(ICameraRaw, handle, &err);
8. Free memory and close:
   ICameraRaw->CameraRaw_FreeMemImage(ICameraRaw, (APTR)img);
   ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
   IExec->DropInterface((struct Interface *)ICameraRaw);
   IExec->CloseLibrary(CameraRawBase);

Full function reference is available in Documentation/libs/cameraraw.library/cameraraw.doc.


6. License & Compliance Info
----------------------------
LibRaw is free software; you can redistribute it and/or modify it under the
terms of either of the following two licenses at your choice:
1. GNU Lesser General Public License version 2.1 (LGPL 2.1).
2. Common Development and Distribution License (CDDL) Version 1.0.

See Documentation/libs/cameraraw.library/ for complete license texts and third-party
copyright details.
