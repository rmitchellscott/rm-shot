XOVI_REPO ?= $(HOME)/github/asivery/xovi
name = rm-shot

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")

CC_AARCH64 ?= aarch64-linux-gnu-gcc
CC_ARMV7 ?= armv7-unknown-linux-gnueabihf-gcc
CFLAGS = -D_GNU_SOURCE -fPIC -O2 -DVERSION=\"$(VERSION)\"
LDFLAGS = -lpthread

# Architecture-specific build directories
BUILD_AARCH64 = build-aarch64
BUILD_ARMV7 = build-armv7

# Architecture-specific objects
OBJECTS_AARCH64 = $(BUILD_AARCH64)/screenshot.o $(BUILD_AARCH64)/xovi.o
OBJECTS_ARMV7 = $(BUILD_ARMV7)/screenshot.o $(BUILD_ARMV7)/xovi.o

.PHONY: all clean clean-aarch64 clean-armv7

all: $(name)-aarch64.so $(name)-armv7.so

# ARM64 (aarch64) build
$(name)-aarch64.so: $(OBJECTS_AARCH64)
	$(CC_AARCH64) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

$(BUILD_AARCH64)/%.o: src/%.c $(BUILD_AARCH64)/xovi.h
	$(CC_AARCH64) $(CFLAGS) -I$(BUILD_AARCH64) -c $< -o $@

$(BUILD_AARCH64)/xovi.o: $(BUILD_AARCH64)/xovi.c $(BUILD_AARCH64)/xovi.h
	$(CC_AARCH64) $(CFLAGS) -I$(BUILD_AARCH64) -c $< -o $@

$(BUILD_AARCH64)/xovi.h: $(BUILD_AARCH64)/xovi.c
$(BUILD_AARCH64)/xovi.c: rm-shot.xovi
	@mkdir -p $(BUILD_AARCH64)
	python3 $(XOVI_REPO)/util/xovigen.py -a aarch64 -o $(BUILD_AARCH64)/xovi.c -H $(BUILD_AARCH64)/xovi.h rm-shot.xovi

# ARM32v7 (armv7) build
$(name)-armv7.so: $(OBJECTS_ARMV7)
	$(CC_ARMV7) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

$(BUILD_ARMV7)/%.o: src/%.c $(BUILD_ARMV7)/xovi.h
	$(CC_ARMV7) $(CFLAGS) -I$(BUILD_ARMV7) -c $< -o $@

$(BUILD_ARMV7)/xovi.o: $(BUILD_ARMV7)/xovi.c $(BUILD_ARMV7)/xovi.h
	$(CC_ARMV7) $(CFLAGS) -I$(BUILD_ARMV7) -c $< -o $@

$(BUILD_ARMV7)/xovi.h: $(BUILD_ARMV7)/xovi.c
$(BUILD_ARMV7)/xovi.c: rm-shot.xovi
	@mkdir -p $(BUILD_ARMV7)
	python3 $(XOVI_REPO)/util/xovigen.py -a armv7 -o $(BUILD_ARMV7)/xovi.c -H $(BUILD_ARMV7)/xovi.h rm-shot.xovi

clean-aarch64:
	rm -f $(name)-aarch64.so $(BUILD_AARCH64)/*.o

clean-armv7:
	rm -f $(name)-armv7.so $(BUILD_ARMV7)/*.o

clean: clean-aarch64 clean-armv7
	rm -rf $(BUILD_AARCH64) $(BUILD_ARMV7)
