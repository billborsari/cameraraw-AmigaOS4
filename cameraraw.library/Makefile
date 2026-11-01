# Makefile for cameraraw.library (Standard and AmigaOne A1222 SPE)

CROSS_COMPILE = ppc-amigaos-
CC     = $(CROSS_COMPILE)gcc
CXX    = $(CROSS_COMPILE)g++
STRIP  = $(CROSS_COMPILE)strip
RM     = rm -f

# Target shared library names
TARGET_STD   = cameraraw.library
TARGET_A1222 = cameraraw.library.a1222

# Source files
SRCS = init.cpp cameraraw_api.cpp
OBJS_STD   = init.o cameraraw_api.o
OBJS_A1222 = init.a1222.o cameraraw_api.a1222.o

# Common includes and defines
INCLUDES = -I.. -I.
DEFINES  = -D__NOLIBBASE__ -D__NOGLOBALIFACE__

# Standard compiler and linker flags
CXXFLAGS_STD = -O2 -Wall -mcrt=newlib -fPIC $(INCLUDES) $(DEFINES)
LDFLAGS_STD  = -mcrt=newlib -nostartfiles -Wl,-q -T cameraraw.ld

# A1222 SPE compiler and linker flags
# -mcpu=8548 targets e500v2 core, -Wa,-me500 instructs assembler to accept SPE opcodes
CXXFLAGS_A1222 = -O3 -mcpu=8548 -Wa,-me500 -Wall -mcrt=newlib -fPIC $(INCLUDES) $(DEFINES)
LDFLAGS_A1222  = -mcpu=8548 -mcrt=newlib -nostartfiles -Wl,-q -T cameraraw.ld

# Static libraries to link
LIBRAW_STD   = ../lib/.libs/libraw.a
LIBRAW_A1222 = ../lib/libraw_a1222.a
LIBJPEG     ?= $(wildcard /opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/lib/libjpeg.a)
ifeq ($(strip $(LIBJPEG)),)
LIBJPEG      = -ljpeg
endif
SYSTEM_LIBS  = -lstdc++ -lz -lm

all: $(TARGET_STD) $(TARGET_A1222) tests

# --- Standard Target ---
standard: $(TARGET_STD)

$(TARGET_STD): $(OBJS_STD) $(LIBRAW_STD)
	$(CXX) $(LDFLAGS_STD) -o $@ $(OBJS_STD) $(LIBRAW_STD) $(LIBJPEG) $(SYSTEM_LIBS)
	$(STRIP) --strip-unneeded $@
	cp -f $@ $(TARGET_STD).std

init.o: init.cpp cameraraw.h
	$(CXX) $(CXXFLAGS_STD) -c $< -o $@

cameraraw_api.o: cameraraw_api.cpp cameraraw.h
	$(CXX) $(CXXFLAGS_STD) -c $< -o $@

# --- A1222 SPE Target ---
a1222: $(TARGET_A1222)

$(TARGET_A1222): $(OBJS_A1222) $(LIBRAW_A1222)
	$(CXX) $(LDFLAGS_A1222) -o $@ $(OBJS_A1222) $(LIBRAW_A1222) $(LIBJPEG) $(SYSTEM_LIBS)
	$(STRIP) --strip-unneeded $@

$(LIBRAW_A1222):
	$(MAKE) -C .. -f Makefile.a1222

init.a1222.o: init.cpp cameraraw.h
	$(CXX) $(CXXFLAGS_A1222) -c $< -o $@

cameraraw_api.a1222.o: cameraraw_api.cpp cameraraw.h
	$(CXX) $(CXXFLAGS_A1222) -c $< -o $@

tests: test-library test-library.a1222

test-library: ../samples/test-library.cpp
	$(CXX) -O2 -mcrt=newlib $< -o ../bin/$@ -lauto
	$(STRIP) --strip-unneeded ../bin/$@

test-library.a1222: ../samples/test-library.cpp
	$(CXX) -O2 -mcpu=8548 -Wa,-me500 -mcrt=newlib $< -o ../bin/$@ -lauto
	$(STRIP) --strip-unneeded ../bin/$@

clean:
	$(RM) $(TARGET_STD) $(TARGET_STD).std $(TARGET_A1222) $(OBJS_STD) $(OBJS_A1222) ../bin/test-library ../bin/test-library.a1222

.PHONY: all standard a1222 tests test-library test-library.a1222 clean
