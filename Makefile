.DEFAULT_GOAL := desktop
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.hpp src/*.inc)
WARN := -Wall -Wextra -Wpedantic
HOST_FLAGS = $(shell pkg-config --cflags sdl2 libavformat libavcodec libswresample libswscale libavutil)
HOST_LIBS = $(shell pkg-config --libs sdl2 libavformat libavcodec libswresample libswscale libavutil)
CODECS := build/codecs
NX := $(DEVKITPRO)/libounce
A64 := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf
NX_ARCH := -mcpu=cortex-a78c+crypto -mtp=soft -fPIE
NX_OBJ := $(patsubst src/%.cpp,build/nx/%.o,$(SOURCES))

.PHONY: desktop switch codecs clean
desktop: build/WinampNX2
switch: build/WinampNX2.nro
codecs: $(CODECS)/lib/libavcodec.a
$(CODECS)/lib/libavcodec.a: scripts/build-codecs.sh
	bash scripts/build-codecs.sh

build/WinampNX2: $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 $(WARN) $(HOST_FLAGS) $(SOURCES) $(HOST_LIBS) -o $@

build/nx/%.o: src/%.cpp $(HEADERS) | $(CODECS)/lib/libavcodec.a
	@mkdir -p build/nx
	$(A64)-g++ -std=c++17 -O2 -g0 -MMD -MP $(WARN) $(NX_ARCH) -D__SWITCH__ -D__SWITCH2__ -isystem $(NX)/include -I$(CODECS)/include -ffunction-sections -fdata-sections -c $< -o $@
-include $(NX_OBJ:.o=.d)

build/WinampNX2.elf: $(NX_OBJ) $(CODECS)/lib/libavcodec.a
	$(A64)-g++ $(NX_ARCH) -specs=$(NX)/switch.specs $(NX_OBJ) -L$(CODECS)/lib -L$(NX)/lib -Wl,--start-group -lavformat -lavcodec -lswresample -lswscale -lavutil -lnx -lm -Wl,--end-group -o $@

build/WinampNX2.nacp: Makefile
	@mkdir -p build
	$(DEVKITPRO)/tools/bin/nacptool --create WinampNX2 "amariscalc" 0.5.0 $@

build/WinampNX2.nro: build/WinampNX2.elf build/WinampNX2.nacp assets/icon.jpg
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --nacp=build/WinampNX2.nacp --icon=assets/icon.jpg

clean:
	rm -rf build/nx build/WinampNX2 build/WinampNX2.elf build/WinampNX2.nacp build/WinampNX2.nro
