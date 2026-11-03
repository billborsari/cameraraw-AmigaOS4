# Native (macOS/Linux) build of the same LibRaw + cameraraw_api.cpp + CLI
# sources, for reference output and host-side profiling.
#   make -C perf/host            -> perf/host/out/{dcraw_emu,test-library}
#   make -C perf/host OPT="-O2 -g -fno-omit-frame-pointer"
# The LibRaw source list is taken from Makefile.std so both stay in step.
ROOT    = ../..
OUT     = out
CXX    ?= c++
OPT    ?= -O2 -g
# LIBRAW_NOTHREADS matches Makefile.std. -ffp-contract=fast: GCC on PPC fuses
# a*b+c into fmadd by default, and with the same contraction clang on arm64
# produces bit-identical output to the Amiga build (checked with raw-bench
# CRCs). With contraction off, convert_to_rgb() rounds differently.
FP     ?= -ffp-contract=fast
DEFS    = -DLIBRAW_NOTHREADS -DHAVE_STDINT_H=1 -DSTDC_HEADERS=1 $(EXTRA_DEFS)
CXXFLAGS = -MMD -MP $(OPT) $(FP) $(DEFS) -I$(ROOT) -I$(ROOT)/libraw -Iinclude -w
SOURCES := $(shell sed -n '/^SOURCES = /,/^$$/p' $(ROOT)/Makefile.std | tr -d '\\' | sed 's/SOURCES = //')
OBJS    = $(patsubst src/%.cpp,$(OUT)/obj/%.o,$(SOURCES))

all: $(OUT)/dcraw_emu $(OUT)/test-library $(OUT)/raw-bench $(OUT)/raw-identify

$(OUT)/libraw_host.a: $(OBJS)
	ar crs $@ $^

$(OUT)/obj/%.o: $(ROOT)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/api.o: $(ROOT)/cameraraw.library/cameraraw_api.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OUT)/host_exec.o: host_exec.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/dcraw_emu: $(ROOT)/samples/dcraw_emu.cpp $(OUT)/api.o $(OUT)/host_exec.o $(OUT)/libraw_host.a
	$(CXX) $(CXXFLAGS) $^ -o $@ -lm
$(OUT)/test-library: $(ROOT)/samples/test-library.cpp $(OUT)/api.o $(OUT)/host_exec.o $(OUT)/libraw_host.a
	$(CXX) $(CXXFLAGS) $^ -o $@ -lm

$(OUT)/raw-bench: $(ROOT)/samples/raw-bench.cpp $(OUT)/api.o $(OUT)/host_exec.o $(OUT)/libraw_host.a
	$(CXX) $(CXXFLAGS) $^ -o $@ -lm

$(OUT)/raw-identify: $(ROOT)/samples/raw-identify.cpp $(OUT)/libraw_host.a
	$(CXX) $(CXXFLAGS) $^ -o $@ -lm

clean:
	rm -rf $(OUT)
.PHONY: all clean
-include $(OBJS:.o=.d)
