# cameraraw-AmigaOS4 on AmigaOS 4.1 (QEMU sam460ex): correctness and performance

This report covers testing `cameraraw.library` 54.2 and its CLI tools
(LibRaw 0.22.1) on an emulated Sam460 (QEMU `sam460ex`, 1 GB, AmigaOS 4.1
FE), compared with a native build of the same sources on macOS/arm64. It
describes the changes on the `perf-qemu` branch (on top of `4c38ac0`),
the problems found that the branch does not change, and the caveats
that come from measuring under emulation. Only the standard PowerPC build
was run. The A1222/SPE build was only compiled (see B1), and nothing was
measured on real hardware.

## Summary

- **The port works.** The library, dcraw_emu, raw-identify, simple_dcraw
  and test-library ran on the guest without crashes. With the current
  adtools GCC 11 / binutils 2.40 toolchain, the build needed two small
  fixes.
- **Output matches a native build.** Decoded raw data is bit-identical to
  the native build for all six test cameras. Final images are
  bit-identical for linear, VNG, PPG, half-size and X-Trans linear
  interpolation, and so are dcraw_emu's PPM output files. AHD and the
  X-Trans Markesteijn demosaic differ in isolated pixels. On the 6 MP file
  that is 1 pixel, off by 1 and 3 levels. This is smaller than the normal
  difference between FMA and non-FMA desktop builds of LibRaw.
- **Where the time goes under QEMU.** A 12 MP CR2 at default settings
  took 66 s, against 0.47 s natively. The emulator spends it in
  floating-point code (`scale_colors`, `convert_to_rgb`, AHD's `cielab()`),
  in small-block file I/O while opening and decoding, and, for X-Trans,
  in the demosaic.
- **Three changes, all bit-identical:**
  - a buffered file stream: open of a 17 MB CR2 3.5 s → 0.3 s;
  - a lookup table in `scale_colors`: 11.9 s → 0.85 s at 24 MP under QEMU;
  - an exact box filter in the X-Trans demosaic: 2× faster on every
    platform.
  On the emulated Sam460 that gives 1.4–1.6× for linear, PPG and
  half-size, and 1.1–1.2× for the default AHD.
- **Problems found (B1–B7), not changed here:**
  - the A1222 SPE flags no longer exist in GCC ≥ 9;
  - `-DLIBRAW_NOTHREADS` makes the shared library unsafe for two
    programs at once;
  - opening the library by path can return a different, already
    resident build;
  - the library is not expunged from memory;
  - a few smaller API points.

## A. Changes in this branch

### A1. Build fixes (adtools GCC 11.5, binutils 2.40, `walkero/amigagccondocker:os4-gcc11`)

| problem | fix |
|---|---|
| Every tool and the library fail to link: `undefined reference to __gthread_once` / `__gthread_setspecific` (from `libgcc_eh`). | Link with `-athread=single` (Makefile.std, Makefile.a1222, cameraraw.library/Makefile). |
| `ld: target elf32-amigaos not found`. binutils 2.40 names the target `elf32-powerpc-amigaos`. | cameraraw.library/Makefile generates `cameraraw.gen.ld` from `cameraraw.ld` with the name that `objdump -i` lists, so older toolchains still work. |
| `make -f Makefile.amiga standard` (autotools) fails to link the tools in this image: libtool picks up `/SDK/local/newlib/lib/libjpeg.so` and tries a static link of `libstdc++.so`. | `standard` builds through Makefile.std, which is also what cameraraw.library links against. The autotools route remains as `standard-autotools`. |
| Makefile.std / Makefile.a1222 have no header dependencies, so a changed header leaves stale objects with an old class layout. | `-MMD -MP` plus `-include` of the `.d` files. |

The standard build passes no `-mcpu`. GCC 11's default is generic
PowerPC: the only FP instructions beyond the base set are `fctiwz` and
`mfcr`, and `objdump -d | grep -c dlmzb` is 0 for every binary.

### A2. Speed-ups (all bit-identical, each behind a define)

| define | what | effect | enabled in |
|---|---|---|---|
| `LIBRAW_USE_BUFFERED_FILE_DATASTREAM` | New `LibRaw_buffered_file_datastream`: plain stdio, its own 64 KB buffer and file position, large reads straight into the caller's buffer. `open_file()` uses it instead of `LibRaw_bigfile_datastream` (LibRaw already does the same on Windows with `LibRaw_bigfile_buffered_datastream`). | The stock stream does a newlib `getc()` per byte with a 1 KB stdio buffer that every `fseeko()` throws away, so metadata parsing and decoding become thousands of small DOS Read/Seek requests. Guest: 450D open 3.97 → 0.30 s, unpack 9.37 → 7.27 s. | Makefile.std, Makefile.a1222 |
| `LIBRAW_SCALE_COLORS_LUT` | `scale_colors_loop()` through a per-channel table over 0..max(`maximum`, `data_maximum`) (8–32 KB per channel for 12–14-bit data); values above it use the original arithmetic. | Classic-FPU PowerPC converts int↔float through memory, three times per sample here. Guest: 24 MP 11.9 → 0.85 s, 12 MP 5.5 → 0.37 s. Natively (SIMD) it is 2.5× *slower*, hence the define. | Makefile.std only. Not for SPE, which converts in registers; untested there. |
| `LIBRAW_XTRANS_BOXSUM` | The 5×5 homogeneity sums in `xtrans_interpolate()` computed once per tile as a separable running box filter (integer sums of the same values). | Native: X-Trans `-q 2` 10.9 → 5.5 s, `-q 3` 15.0 → 7.4 s (24 MP). Guest −5 %, because float work dominates there. Plain C, so it would help upstream LibRaw on every platform. | Makefile.std, Makefile.a1222 |
| `LIBRAW_XTRANS_INTCMP` | Optional. Markesteijn's `drv <= tr` float comparisons done on order-preserving integer keys. | Guest: X-Trans `-q 2` interpolation 183.8 → 164.4 s. Native: 20 % slower; generic PPC needs `mfcr` for the branch-free count. | Off by default, until measured on hardware. |

`LibRaw_buffered_file_datastream` keeps the semantics of the stdio stream
it replaces:
- `gets()` follows `fgets()`, including newlib's `NULL` for size < 2;
- `scanf_one()` follows `fscanf("%d"/"%f")`, including pushing back a
  failed prefix;
- `eof()` follows `feof()`, set by a read that ran into the end and
  cleared by a seek.
One deliberate difference: newlib's `fseeko()` on AmigaOS refuses to seek
past the end of file (and leaves the position unchanged), while POSIX
stdio, desktop LibRaw and the new stream allow it. That only affects
malformed files.

Tried and rejected, with numbers in Performance:
- an exact ushort→float lookup table in `cielab()`/`convert_to_rgb`:
  2× *slower* under QEMU;
- caching the cbrt table: no measurable gain;
- `-O3`: ~3 % for a library 8.5 % bigger;
- `-ffast-math`: no native gain, and it changes output.

### A3. Measurement tools (no effect on the Amiga build)

- `samples/raw-bench.cpp`: opens the library like an application and
  times each step: open, unpack, `dcraw_process` (split by LibRaw's
  progress callbacks into scale_colors / pre-interpolate / interpolate /
  convert_to_rgb), and `make_mem_image`. It samples `AvailMem()` for peak
  memory and prints endian-neutral CRC-32s of the raw data and the final
  image, so Amiga and desktop output can be compared without copying
  images. Options: `-q`, `-h`, `-6`, `-4`, `-T`, `-o` (also write
  PPM/TIFF, timed), `-m` (read into memory, use `OpenBuffer`), `-L`
  (library path), `-c` (CRC after every stage), `-R` (CRC per output
  row), `-d` (dump RGB).
- `perf/host/`: builds the same LibRaw sources, `cameraraw_api.cpp`,
  dcraw_emu, test-library and raw-bench natively (macOS/Linux) against
  small stand-in exec headers, for reference output and profiling.
  `cameraraw_api.cpp` skips its PPC stack-swap asm when not on AmigaOS,
  and `stream_test.cpp` is the randomised equivalence test for the new
  stream.
- `perf/build-variant.sh`: library variants with different compile flags
  for A/B tests. Each gets its own library name through
  `CAMERARAW_LIBNAME` (init.cpp) because of B3.
- `perf/tlat.c`: timer.device lateness probe, to check that the emulated
  machine is healthy before timing.
- `perf/memcpy_test.c`: large memcpy/memmove check (see Correctness).

## Test setup

Test files, all **CC0 1.0** from raw.pixls.us. SHA-256 checksums were
checked against the site.

| file | camera | MP | decoder | URL |
|---|---|---|---|---|
| CR2 | Canon EOS 450D | 12.2 | lossless JPEG | https://raw.pixls.us/getfile.php/1353/nice/Canon%20-%20EOS%20450D%20-%20RAW%20(3:2).CR2 |
| NEF | Nikon D70 (2004) | 6.1 | Nikon lossy type 1 | https://raw.pixls.us/getfile.php/2060/nice/Nikon%20-%20D70%20-%2012bit%2012bit%20compressed%20(Lossy%20(type%201))%20(3:2).NEF |
| NEF | Nikon D750 | 24.3 | Nikon 14-bit lossless | https://raw.pixls.us/getfile.php/898/nice/Nikon%20-%20D750%20-%2014bit%2014bit%20compressed%20(Lossless)%20(3:2).NEF |
| ARW | Sony ILCE-7M3 | 24.3 | ARW2 compressed | https://raw.pixls.us/getfile.php/2414/nice/Sony%20-%20ILCE-7M3%20-%2014bit%2014bit%20compressed%20(3:2).ARW |
| RAF | Fujifilm X-T2 | 24.4 | X-Trans compressed | https://raw.pixls.us/getfile.php/865/nice/Fujifilm%20-%20X-T2%20-%2014bit%2014bit%20compressed%20(3:2).RAF |
| DNG | Ricoh GR II | 16.3 | DNG lossless JPEG | https://raw.pixls.us/getfile.php/1955/nice/Ricoh%20-%20GR%20II%20-%2012bit%20(3:2).DNG |

- **Native:** all six files, with `-q 0/1/2/3/4/11/12`, `-h`, `-6 -q 0`
  and `-4 -q 2`.
- **Guest:** D70 and 450D from a FFS hard file (emulated IDE), ARW and
  RAF from RAM:. The dev disk had too little space for the others. Guest
  runs alternated the stock and the changed library, case by case, in
  the same session.
- **Tools:** native reference built with clang `-O2`, profiled with
  Instruments' Time Profiler. The guest was driven through Amiga DevBench
  (a host-side test bridge).

## Correctness

### Amiga vs native

The two agree bit-for-bit **when the native build uses the same
floating-point contraction as PowerPC GCC**. GCC fuses `a*b+c` into
`fmadd`/`fmadds` by default; `perf/host` therefore builds with
`-ffp-contract=fast`. Without that, `convert_to_rgb()` rounds differently.

| check | result |
|---|---|
| decoded raw data, all six decoders | identical |
| final image, `-q 0` linear, `-q 1` VNG, `-q 2` PPG, `-h`; X-Trans `-q 0`, `-h` | identical |
| final image, `-q 3` AHD; X-Trans `-q 2/-q 3` Markesteijn | isolated pixels differ (below); identical across all Amiga builds |
| dcraw_emu on the Amiga (via the library): 450D `-q 0` PPM, D70 `-6 -q 0` 16-bit PPM, D70 `-h` PPM | byte-identical to native |
| dcraw_emu `-h -T` TIFF | identical pixels. The header is `MM` on the Amiga and `II` natively: LibRaw writes TIFF in host byte order |
| simple_dcraw `-e` thumbnail | identical JPEG data. The EXIF header LibRaw adds is in host byte order |
| raw-identify `-v` | identical apart from the file name |
| test-library | runs; 450D 54.4 s, D70 27.4 s with the changed library |

AHD / Markesteijn differences:
- D70 `-q 3`, compared in full: 2 of 18,361,638 samples differ, in one
  pixel, by 1 and 3 levels. AHD picked a different direction there.
- Per-row CRCs: 450D AHD has a difference in 12 of 2,856 rows, Sony AHD in
  10 of 4,024, Fuji `-q 2` in 94 of 4,028. The rows are scattered.
- Ruled out:
  - libm: the 65,536-entry `pow()` cube-root table has the same CRC on
    both;
  - FMA contraction or double rounding in `cielab()`;
  - `char` signedness;
  - uninitialised buffers (AHD's come from `calloc`).
- Not isolated: possibly a one-ulp difference in the Lab conversion, or
  QEMU's softfloat.

For scale, an FMA build (= PowerPC) against a non-FMA build (typical
x86-64), native, default `-q 3`, 8-bit output:

| file | samples differing | max | mean |
|---|---|---|---|
| D70 | 272 (0.0015 %) | 5 | 0.000018 |
| 450D | 383 (0.0010 %) | 8 | 0.000017 |
| Sony A7 III | 614 (0.0008 %) | 31 | 0.000017 |
| Fuji X-T2 | 16,548 (0.0227 %) | 23 | 0.000303 |

### Changed vs stock

- **Native:** 60 cases (6 files × 10 option sets) have identical raw and
  image CRCs with and without A2.
- **Amiga:** every A/B run below has identical CRCs.
- **Stream equivalence:** `perf/host/stream_test.cpp` runs 100k–300k
  random get_char / read / seek (SET/CUR/END) / tell / eof / gets / scanf
  calls on the new and the stock stream over the same file. It passes
  natively and on the Amiga (newlib), with RAW files and a text file of
  awkward numbers.

### newlib memcpy

A memcpy problem with blocks of ~300 KB and more had been seen on this
QEMU setup before. `perf/memcpy_test.c` (64 KB–6 MB blocks, several
alignments, memcpy and memmove) found **0 of 72 copies wrong** on this
guest. The output agrees: X-Trans's 1.5 MB tile copies would corrupt
contiguous groups of rows if they lost data, and the differences above
are scattered single pixels instead. Nothing here works around it.
The new stream copies at most 64 KB with `memcpy`.

## Performance

### Native reference (Apple M-series, one core, clang -O2)

`dcraw_process` in ms, plus unpack. "→" marks the value after A2 where it
changed by more than 15 %. On X-Trans, `-q 3` and above run the 3-pass
Markesteijn. Max RSS: 79 MB at 6 MP, 156–162 MB at 12 MP, 304–329 MB at
24 MP.

| file | unpack | -q 0 linear | -q 1 VNG | -q 2 PPG | -q 3 AHD (default) | -q 4 DCB | -q 11 DHT | -q 12 AAHD | -h |
|---|---|---|---|---|---|---|---|---|---|
| nikon_d70_lossy.nef | 49 | 56 | 372 | 47 | 137 | 222 | 413 | 569 | 15 |
| canon_450d.cr2 | 141 | 109 | 808 | 94 | 261 | 470 | 904 | 1257 | 28 |
| ricoh_grii.dng | 158 | 92 | 698 | 77 | 233 | 432 | 782 | 1074 | 25 |
| nikon_d750_14lossless.nef | 175 | 228 | 1619 | 184 | 529 | 918 | 1893 | 2486 | 57 |
| sony_a7m3_14comp.arw | 54 | 222 | 1623 | 191 | 519 | 925 | 1711 | 2496 | 58 |
| fuji_xt2_14comp.raf | 544 | 229 | 1539 | 10872 → 5516 | 14950 → 7399 | 14925 → 7263 | 14919 → 7257 | 14947 → 7247 | 61 |

Native hotspots:
- **Bayer AHD:** about two thirds of the work is AHD, and a quarter of
  that is `cielab()`. `convert_to_rgb` is ~7 %, `raw2image`/`copy_bayer`
  ~8 %, the decoder ~9 %.
- **450D CR2:** the lossless-JPEG bit pump (a virtual `get_char()` per
  byte) is ~27 %.
- **X-Trans `-q 2/3`:** `xtrans_interpolate()` is 91 %. Within it, the
  homogeneity map (float compares) 27 %, the 5×5 sums 23 % (fixed by
  BOXSUM), the averaging 16 %.

### Amiga (QEMU sam460ex), stock → changed

Seconds. `interpolate` covers everything between pre-interpolate and
convert_to_rgb. "probe late" is the timer.device lateness measured just
before the case, as a health check (see C). All image CRCs are equal
before and after. Run-to-run noise is about ±10 % per stage: for example,
the Fuji unpack from RAM: varies between 6.9 and 8.8 s with identical
code.

| file, options | probe late (ms) | open | unpack | scale_colors | interpolate | convert_to_rgb | **total** | speed-up |
|---|---|---|---|---|---|---|---|---|
| D70 6 MP NEF (DH1:) `-q 0` | 12 | 0.87 → 0.48 | 3.23 → 1.99 | 2.62 → 0.16 | 0.49 → 0.47 | 3.93 → 3.96 | **12.0 → 7.9** | 1.52× |
| D70 6 MP NEF (DH1:) `-q 2` | 14 | 0.82 → 0.30 | 2.83 → 1.95 | 2.94 → 0.23 | 0.69 → 0.66 | 4.27 → 3.85 | **12.6 → 8.0** | 1.56× |
| D70 6 MP NEF (DH1:) `-q 3` | 12 | 0.82 → 0.32 | 3.15 → 2.12 | 3.00 → 0.16 | 20.58 → 20.72 | 3.91 → 3.99 | **32.4 → 28.2** | 1.15× |
| D70 6 MP NEF (DH1:) `-h` | 18 | 0.84 → 0.35 | 2.94 → 2.07 | 0.71 → 0.06 | 0.01 → 0.01 | 0.93 → 0.98 | **6.1 → 4.1** | 1.47× |
| 450D 12 MP CR2 (DH1:) `-q 0` | 14 | 3.61 → 0.17 | 8.70 → 7.30 | 5.49 → 0.37 | 0.85 → 0.90 | 7.55 → 7.65 | **27.8 → 17.9** | 1.55× |
| 450D 12 MP CR2 (DH1:) `-q 1` | 12 | 3.51 → 0.20 | 9.91 → 7.51 | 5.84 → 0.44 | 12.49 → 12.98 | 7.53 → 7.82 | **40.8 → 30.5** | 1.34× |
| 450D 12 MP CR2 (DH1:) `-q 2` | 8 | 3.20 → 0.30 | 9.08 → 6.97 | 5.73 → 0.44 | 1.29 → 1.46 | 7.89 → 7.63 | **28.9 → 18.5** | 1.56× |
| 450D 12 MP CR2 (DH1:) `-q 3` | 9 | 3.46 → 0.29 | 8.98 → 7.34 | 5.66 → 0.42 | 38.67 → 36.21 | 7.61 → 7.77 | **65.9 → 53.7** | 1.23× |
| 450D 12 MP CR2 (DH1:) `-h` | 12 | 3.41 → 0.26 | 9.17 → 7.11 | 1.69 → 0.13 | 0.06 → 0.04 | 2.02 → 1.95 | **17.3 → 10.6** | 1.64× |
| A7 III 24 MP ARW (RAM:) `-q 0` | 13 | 0.03 → 0.01 | 1.42 → 1.61 | 11.88 → 0.85 | 1.96 → 1.93 | 15.42 → 15.38 | **34.7 → 23.9** | 1.45× |
| A7 III 24 MP ARW (RAM:) `-q 3` | 11 | 0.01 → 0.02 | 1.54 → 1.61 | 11.55 → 0.79 | 103.21 → 101.98 | 15.90 → 15.34 | **136.4 → 123.7** | 1.10× |
| A7 III 24 MP ARW (RAM:) `-h` | 11 | 0.04 → 0.01 | 1.53 → 1.31 | 3.04 → 0.32 | 0.11 → 0.04 | 4.02 → 3.95 | **11.2 → 8.1** | 1.39× |
| X-T2 24 MP RAF (RAM:) `-h` | 11 | 0.01 → 0.01 | 7.51 → 8.83 | 3.04 → 0.24 | 0.00 → 0.02 | 3.63 → 3.69 | **16.1 → 14.5** | 1.11× |
| X-T2 24 MP RAF (RAM:) `-q 0` | 15 | 0.01 → 0.00 | 7.15 → 8.13 | 10.91 → 0.90 | 1.94 → 1.68 | 15.05 → 14.11 | **37.8 → 27.4** | 1.38× |
| X-T2 24 MP RAF (RAM:) `-q 2` | 15 | 0.01 → 0.00 | 8.10 → 6.85 | 11.02 → 0.77 | 193.07 → 185.75 | 14.88 → 15.06 | **229.0 → 210.9** | 1.09× |

Peak memory, sampled with `AvailMem()` and the same before and after:

| size | full size | half size |
|---|---|---|
| 6 MP | 78 MB, 84 MB with AHD | 30 MB |
| 12 MP | 154–160 MB | 58 MB |
| 24 MP | 303–316 MB | 112 MB |

Where a stock 12 MP CR2 at `-q 3` (66 s) spends its time:

| stage | share | guest/native |
|---|---|---|
| open/identify (hard file) | 5 % | ~6000× |
| unpack (decode + I/O) | 14 % | ~65× |
| scale_colors | 9 % | ~1000× |
| AHD | 59 % | ~210× |
| convert_to_rgb | 12 % | ~350× |

Reading the 17.6 MB CR2 with a single `fread()` took 8.1 s from the FFS
hard file. With the file already in memory (`raw-bench -m`), unpack took
3.0 s instead of 9.4 s.

Single-change attribution (Amiga):

| change | numbers |
|---|---|
| buffered stream | 450D `-q 0`: open 3.97 → 0.30 s, unpack 9.37 → 7.27 s |
| scale_colors table | A7 III `-q 0`: scale_colors 11.40 → 0.86 s |
| X-Trans box filter | X-T2 `-q 2`: interpolation 193.8 → 183.9 s |
| X-Trans integer compares (off by default) | X-T2 `-q 2`: 183.8 → 164.4 s |
| ushort→float table (rejected) | D70 `-q 3`: dcraw_process 26.8 → 49.1 s; A7 III `-q 0`: 30.0 → 38.0 s |
| cbrt cache (rejected) | D70 `-q 3`: 26.8 → 27.2 s |
| `-O3` (rejected) | 450D `-q 3`: 52.2 → 50.4 s |
| no `LIBRAW_NOTHREADS` (B2) | 450D `-q 3`: 52.2 → 50.8 s, identical output |

## B. Problems found (not changed in this branch)

**B1. A1222 / SPE build.**
- **It doesn't build with current GCC.** `-mabi=spe -mspe=yes
  -mfloat-gprs=double` are rejected by GCC 11, because GCC removed the
  SPE back end in version 9. With only `-mcpu=8548 -Wa,-me500`, LibRaw
  compiles to 6,195 classic-FPU instructions (`lfs`, `fmadds`, `fctiwz`,
  …) and no SPE instructions. Those would trap and be emulated on the
  e500v2, which is what the A1222 build is meant to avoid.
- **The float ABI may not match.** If the release binaries come from an
  older SPE-capable GCC, it is worth checking that the C library and libm
  they call use the same SPE float ABI. With `-mfloat-gprs=double`,
  floats travel in GPRs, so a classic-FPU `pow()`, `atof()` or
  `printf("%f")` would see wrong values.
- **Suggestion:** document the required compiler, and add a small
  float-ABI test to test-library. Not tested here: there is no e500
  hardware, and QEMU's 460 has no SPE.

**B2. Shared-library reentrancy.**
- `-DLIBRAW_NOTHREADS` turns LibRaw's per-object decoder state into
  `static` variables: `getbithuff()`'s bit buffer, `ph1_bithuff`, the
  Sony and medium-format decoders, and `cielab()`'s tables. In an AmigaOS
  library those statics are shared by every program that has
  cameraraw.library open, so two programs decoding at once can corrupt
  each other.
- **Suggested fix:** build without `-DLIBRAW_NOTHREADS`. LibRaw then keeps
  this state per object (`tls`) and needs no thread library. It builds
  with this toolchain, gives identical output, and showed no slowdown
  (above).

**B3. `OpenLibrary()` by path returns the resident library.**
- With `cameraraw.library` already in memory, opening another file by
  path (for example `PROGDIR:cameraraw.library.a1222`) returned the
  resident one. Its `lib_IdString` still showed the first build's
  timestamp, and test-library printed `Opened library:
  LIBS:cameraraw.library` although nothing was in LIBS:.
- Every build has the node name `cameraraw.library`, so comparing
  standard against A1222 with test-library can silently measure the same
  library twice.
- **Suggestion:** print `lib_IdString` in test-library, or give the A1222
  build its own name.

**B4. The library is never expunged.**
- After `avail flush` it stayed in memory, with open count 0 and no
  `LIBF_DELEXP`. Updating it therefore needs a reboot.
- **Suggestion:** check how `libExpunge` is reached, and have `libClose`
  expunge when the last user closes with `LIBF_DELEXP` set (the usual
  pattern).

**B5. Stack.**
- Only `Unpack` and `Process` run on the private 1 MB stack. `OpenFile`
  (all the metadata parsing), `UnpackThumb`, `MakeMemImage` and
  `PpmTiffWriter` run on the caller's stack, which can be 64 KB.
- The inline asm in `RunOnLargeStack()` does not list the volatile FPRs,
  CR fields or XER as clobbered. That is harmless today only because
  nothing floating-point is live across the call.
- **Suggestion:** route the other entry points through the large stack
  too, and complete the clobber list.

**B6. dcraw_emu reaches into LibRaw.**
- It casts the handle to `LibRaw *` and writes `imgdata.params`, so a
  dcraw_emu binary only works with a library built from the same
  `libraw.h`. (This branch adds only a non-virtual method, so the class
  layout is unchanged.)
- **Suggestion:** add setter functions to the API.

**B7. Memory.**
- Peak use is about 13 bytes per pixel: raw data 2, `image` 8, and the
  `MakeMemImage` copy 3.
- `MakeMemImage` allocates a full RGB copy (73 MB at 24 MP), which
  applications typically copy again.
- **Suggestions:**
  - wrap LibRaw's `copy_mem_image(buf, stride, bgr)` as an API call that
    renders straight into the caller's buffer;
  - add an option to free the raw buffer after `Process`.

## C. QEMU caveats

- **Floating-point cost is exaggerated.** Under TCG, every PowerPC FP
  instruction is a helper call. Float-heavy stages run 300–1000× slower
  than native, integer code 15–60×. So the Amiga profile above
  overweights `scale_colors`, `convert_to_rgb` and `cielab()`. On a
  Sam460, X5000 or A1222 those stages should be a much smaller share.
- **What should carry over to hardware.**
  - *Buffered stream:* yes. Each 1 KB refill and each seek is still a DOS
    packet to the filesystem. The absolute gain will be smaller without
    IDE emulation, which here manages 2–5 MB/s.
  - *scale_colors table:* probably, on classic-FPU machines. Int↔float
    conversions go through memory with load-hit-store stalls on
    G3/G4/e5500. Expect perhaps 2–3× for that stage rather than QEMU's 12×.
  - *X-Trans box filter:* yes. It is algorithmic.
- **The rejected ushort→float table may be a 440 effect.** It made the
  guest 2× slower. The likely cause is TLB pressure from a second 256 KB
  table next to `cielab()`'s: the 440 has a 64-entry, software-loaded TLB,
  and QEMU emulates that. A real Sam460 may see the same. A G4 or e5500
  might not.
- **Emulator health.**
  - *Host throttling:* during this work macOS App Nap throttled the QEMU
    process at times. All its threads sat at background priority, guest
    timer wake-ups were 40–90 ms late, and file copies into the guest
    slowed by 3×.
  - *Fix applied:* QEMU's QoS was raised (`taskpolicy -t 0 -l 0`) at
    22:40 PST on 2 Nov. All Amiga timings in this report were taken after
    that, alternating stock and changed library, with the probe 8–18 ms
    late.
  - *Probe above target:* that is above the ~5 ms seen once on an idle,
    fresh boot (1.3 ms). It was steady across the session, and A/B pairs
    ran back-to-back, so the ratios hold. The absolute guest times may be
    somewhat pessimistic.
  - *Covered vs visible window:* not compared directly.

## Suggestions

1. Merge A1 and A2. Upstream LibRaw might also be interested in
   `LIBRAW_XTRANS_BOXSUM` and the portable buffered stream.
2. Fix B2 (drop `-DLIBRAW_NOTHREADS`), B3 and B4.
3. Settle the A1222 compiler and ABI (B1) on real hardware.
4. API additions from B5–B7.
5. Measure on real hardware with `raw-bench`. Its stage split and CRCs
   make the numbers comparable with the ones here.
6. Not done, because exactness would be lost: fixed-point
   `convert_to_rgb`/`cielab`. They would be much faster under QEMU and
   on in-order PowerPC, but the output changes (±1 in 16-bit values,
   occasional AHD direction flips). It needs an agreed tolerance.

## Reproducing

```sh
# Amiga build (adtools GCC 11 image)
make -f Makefile.std -j8 all tools
make -C cameraraw.library standard test-library raw-bench

# native reference, profiling, stream test (macOS/Linux)
make -C perf/host
make -C perf/host OUT=/tmp/opt EXTRA_DEFS="-DLIBRAW_USE_BUFFERED_FILE_DATASTREAM -DLIBRAW_SCALE_COLORS_LUT -DLIBRAW_XTRANS_BOXSUM"
c++ -O2 -DLIBRAW_NOTHREADS -I. -Ilibraw -Iperf/host/include perf/host/stream_test.cpp /tmp/opt/libraw_host.a -o stream_test
./stream_test FILE 300000 7

# A/B library variant, e.g. the stock code
perf/build-variant.sh base -O2       # -> perf/variants/crv_base.library
```

On the Amiga:

```
tlat 25                                   ; timer health
raw-bench FILE                            ; per-stage times + CRCs
raw-bench -L crv_base.library -q 3 FILE   ; a variant
stream_test FILE 100000 7 inside          ; "inside": no seeks past EOF (newlib refuses them)
```
