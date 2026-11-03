# Makefile for AmigaOS 4 architecture detection tool
CROSS_COMPILE = ppc-amigaos-
CC     = $(CROSS_COMPILE)gcc
STRIP  = $(CROSS_COMPILE)strip
CFLAGS = -O2 -mcrt=newlib -Wall
TARGET = detect_a1222

all: $(TARGET)

$(TARGET): detect_a1222.c
	$(CC) $(CFLAGS) $< -o $@
	$(STRIP) --strip-unneeded $@

clean:
	rm -f $(TARGET)

.PHONY: all clean

