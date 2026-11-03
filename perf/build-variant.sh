#!/bin/bash
# Build a cameraraw.library variant with different LibRaw compile flags, for
# A/B timing on the guest:  perf/build-variant.sh NAME "extra CXXFLAGS"
# Run inside the walkero toolchain container from the repo root. Output:
# perf/variants/crv_NAME.library (stripped), objects in build_NAME/.
set -e
NAME=$1; EXTRA=$2
DEFS='-DPACKAGE_NAME="LibRaw" -DPACKAGE_TARNAME="libraw" -DPACKAGE_VERSION="0.22.1" -DHAVE_STDIO_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_STRINGS_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_UNISTD_H=1 -DSTDC_HEADERS=1 -DHAVE_DLFCN_H=1 -DHAVE_JPEGLIB_H=1 -DLIBRAW_NOTHREADS'
make -f Makefile.std -j8 BUILD_DIR=build_$NAME TARGET=lib/libraw_$NAME.a \
  CXXFLAGS="-mcrt=newlib -fPIC $DEFS -I. -I./libraw -w $EXTRA" lib/libraw_$NAME.a >/dev/null
mkdir -p perf/variants
cd cameraraw.library
rm -f init.o cameraraw_api.o
# Its own library name (crv_NAME.library): OpenLibrary("path/x.library") on
# OS4 returns an already resident library of the same name instead of
# loading the file, so A/B copies must not all be called cameraraw.library.
LIB=crv_$NAME.library
make LIBRAW_STD=../lib/libraw_$NAME.a TARGET_STD=../perf/variants/$LIB \
  CXXFLAGS_STD="-O2 -Wall -mcrt=newlib -fPIC -I.. -I. -D__NOLIBBASE__ -D__NOGLOBALIFACE__ -DCAMERARAW_LIBNAME=\\\"$LIB\\\"" \
  ../perf/variants/$LIB 2>&1 | grep -iE "error|undefined" || true
rm -f init.o cameraraw_api.o ../perf/variants/$LIB.std
echo "$NAME dlmzb=$(ppc-amigaos-objdump -d ../perf/variants/$LIB | grep -c dlmzb) size=$(stat -c%s ../perf/variants/$LIB) name=$(strings ../perf/variants/$LIB | grep -m1 crv_)"
