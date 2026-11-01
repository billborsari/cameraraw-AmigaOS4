# cameraraw-AmigaOS4

**cameraraw-AmigaOS4** is an AmigaOS 4.x port of [LibRaw](https://www.libraw.org) 0.22.1, providing native camera RAW image decoding, metadata extraction, and demosaicing for over 1,100 digital camera formats.

This project packages LibRaw as:
1. **`cameraraw.library`** – A native AmigaOS 4 shared library (`.library`) offering a clean C/C++ interface for Amiga applications and datatypes.
2. **Command-Line Tools** – AmigaOS 4 CLI utilities including `dcraw_emu`, `raw-identify`, `simple_dcraw`, and `test-library`.
3. **Dual Architecture Builds** – Native builds optimized for:
   - **Standard PowerPC (Classic FPU)**: Target for AmigaOne X5000, X1000, Sam460ex, Pegasos II, and classic G3/G4.
   - **AmigaOne A1222 (Tabor e500v2 SPE)**: Target built with `-mcpu=8548 -Wa,-me500 -O3`, emitting 100% native Signal Processing Engine (SPE) paired SIMD instructions to bypass kernel illegal instruction trap emulation.
4. **Automated Release Packaging** – Scripted installer generation, MD5 hash verification, and archive creation.

---

## Repository Directory Layout

```
.
├── cameraraw.library/      # Native AmigaOS 4 shared library source & linker scripts
│   ├── cameraraw.h         # Public C/C++ developer header file
│   ├── cameraraw.xml       # AmigaOS 4 XML Interface definition
│   ├── cameraraw.ld        # Custom GNU ld linker script for shared library
│   ├── cameraraw_api.cpp   # Exported library API implementation
│   ├── init.cpp            # AmigaOS 4 library initialization boilerplate
│   ├── cameraraw.doc       # AutoDoc API reference manual
│   └── Makefile            # Shared library build script (standard & A1222)
├── libraw/                 # LibRaw 0.22.1 public header files
├── src/                    # LibRaw 0.22.1 core source files (decoders, demosaic, etc.)
├── samples/                # Command-line tools and utilities
│   ├── dcraw_emu.cpp       # Full-featured dcraw emulator CLI
│   ├── raw-identify.cpp    # RAW metadata and CFA pattern inspector CLI
│   ├── simple_dcraw.cpp    # Lightweight RAW unpacker CLI
│   └── test-library.cpp    # Test and benchmark utility for cameraraw.library
├── tools/                  # Hardware detection and build utilities
│   ├── detect_a1222.c      # Runtime AmigaOne A1222 hardware detection tool
│   └── Makefile            # Detection tool makefile
├── release/                # Release structure, installer script, icons, and docs
│   ├── CameraRawLib_54.1/  # Release staging tree (binaries excluded from Git)
│   │   ├── Install cameraraw.library       # Python installer script for AmigaOS 4
│   │   ├── Install cameraraw.library.info  # Installer icon
│   │   ├── Tools.info                      # Tools folder icon
│   │   ├── readme.txt                      # User release documentation
│   │   ├── dcraw_emu.txt                   # CLI usage documentation
│   │   ├── Documentation/                  # AutoDocs and legal licenses
│   │   └── include/                        # SDK developer headers and XML
│   └── CameraRawLib_54.1.info              # Release drawer icon
├── doc/                    # Additional documentation and AutoDocs
├── Makefile.amiga          # Unified top-level makefile for all targets
├── Makefile.a1222          # Standalone makefile for A1222 SPE LibRaw static library
├── update_release.py       # Release packaging tool (populates release/, computes MD5s, builds .lha)
├── COPYRIGHT               # LibRaw copyright notices
├── LICENSE.LGPL            # GNU Lesser General Public License v2.1
├── LICENSE.CDDL            # Common Development and Distribution License v1.0
└── README.LibRaw.md        # Original upstream LibRaw 0.22.1 documentation
```

---

## Building from Source

### Prerequisites
- Cross-compiler toolchain for AmigaOS 4 (`ppc-amigaos-gcc`, `ppc-amigaos-g++`, binutils).
- AmigaOS 4 SDK with `newlib`, `libjpeg.a`, and `zlib`.
- Python 3 (for release packaging).
- `lha` (optional, for building the `.lha` release archive).

### Quick Build (All Targets)

To compile standard PowerPC and A1222 builds, shared libraries, CLI tools, and detection tools:

```bash
make -f Makefile.amiga all
```

### Building Individual Components

1. **Standard PowerPC LibRaw & Tools**:
   ```bash
   make -f Makefile.amiga standard
   ```

2. **AmigaOne A1222 SPE LibRaw & Tools**:
   ```bash
   make -f Makefile.amiga a1222
   ```

3. **Shared Library (`cameraraw.library`)**:
   ```bash
   make -f Makefile.amiga library
   ```
   Or directly inside `cameraraw.library/`:
   ```bash
   cd cameraraw.library
   make all
   ```

4. **Architecture Detection Tool (`detect_a1222`)**:
   ```bash
   make -f Makefile.amiga tools
   ```

---

## Packaging Releases

The release directory `release/CameraRawLib_54.1` maintains the exact structure expected by the AmigaOS 4 installer. Existing binary releases and archives are excluded from the repository.

To populate the release directory with newly compiled binaries, calculate verified MD5 checksums into the Python installer script, and package the archive:

```bash
python3 update_release.py
```

---

## Developer Integration Guide

To use `cameraraw.library` in your AmigaOS 4 C/C++ applications:

```c
#include <proto/exec.h>
#include <exec/libraries.h>
#include <libraries/cameraraw.h>

struct Library *CameraRawBase = IExec->OpenLibrary("cameraraw.library", 54);
if (!CameraRawBase) {
    /* Handle error */
    return;
}

struct CameraRawIFace *ICameraRaw = (struct CameraRawIFace *)IExec->GetInterface(
    CameraRawBase, "main", 1, NULL);

if (ICameraRaw) {
    /* 1. Allocate context handle */
    APTR handle = ICameraRaw->CameraRaw_Init(ICameraRaw, 0);

    /* 2. Open RAW file */
    if (ICameraRaw->CameraRaw_OpenFile(ICameraRaw, handle, "photo.nef") == 0) {
        /* 3. Unpack and process (demosaic) */
        ICameraRaw->CameraRaw_Unpack(ICameraRaw, handle);
        ICameraRaw->CameraRaw_Process(ICameraRaw, handle);

        /* 4. Obtain decoded RGB raster */
        int err = 0;
        void *img = ICameraRaw->CameraRaw_MakeMemImage(ICameraRaw, handle, &err);
        if (img) {
            /* Process or render bitmap */
            ICameraRaw->CameraRaw_FreeMemImage(ICameraRaw, img);
        }
    }

    /* 5. Clean up */
    ICameraRaw->CameraRaw_Close(ICameraRaw, handle);
    IExec->DropInterface((struct Interface *)ICameraRaw);
}

IExec->CloseLibrary(CameraRawBase);
```

For full API documentation, see [`cameraraw.library/cameraraw.doc`](file:///opt/code/LibRaw-0.22.1/cameraraw.library/cameraraw.doc).

---

## Testing & Benchmarks

The `test-library` command measures performance across decoding phases with millisecond precision:

```bash
# Test using system-installed library:
test-library photo.cr2

# Test a specific library build:
test-library photo.cr2 PROGDIR:cameraraw.library.a1222
```

---

## Licensing, Legal Compliance & Attribution

This project is free and open-source software, distributed under the same terms as upstream LibRaw. You may redistribute and/or modify it under the terms of either:

1. **GNU Lesser General Public License version 2.1 (LGPL 2.1)** – See [`LICENSE.LGPL`](file:///opt/code/LibRaw-0.22.1/LICENSE.LGPL)
2. **Common Development and Distribution License version 1.0 (CDDL 1.0)** – See [`LICENSE.CDDL`](file:///opt/code/LibRaw-0.22.1/LICENSE.CDDL)

### Third-Party Software and Copyright Notices

- **LibRaw**: Copyright © 2008–2025 LibRaw LLC (http://www.libraw.org, `info@libraw.org`).
- **dcraw.c**: RAW photo decoder by Dave Coffin, Copyright © 1997–2018 Dave Coffin (RESTRICTED code is not used).
- **DCB Demosaic and FBDD Denoise**: Copyright © 2010 Jacek Gozdz (BSD-3-Clause).
- **X3F Unpacker**: Unpacking Foveon files, Copyright © 2010 Roland Karlsson (BSD-style).
- **Adobe DNG SDK 1.4**: Portions Copyright © 2005 Adobe Systems Incorporated (MIT License).
- **AmigaOS 4 Port & cameraraw.library Wrapper**: Copyright © 2026 Bill Borsari.
