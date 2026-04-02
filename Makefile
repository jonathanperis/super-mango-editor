UNAME := $(shell uname -s)

ifeq ($(findstring MINGW,$(UNAME))$(findstring MSYS,$(UNAME)),)
# macOS / Linux — use whatever sdl2-config is on PATH
CC      = clang
SDL2CFG = sdl2-config
else
# Windows (MSYS2 UCRT64) — pin to the MSYS2 installation
CC      = /c/msys64/ucrt64/bin/clang.exe
SDL2CFG = /c/msys64/ucrt64/bin/sdl2-config
endif

CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell $(SDL2CFG) --cflags)
LIBS    = $(shell $(SDL2CFG) --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
OUTDIR  = out
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c) \
          $(wildcard $(SRCDIR)/collectibles/*.c) \
          $(wildcard $(SRCDIR)/core/*.c) \
          $(wildcard $(SRCDIR)/effects/*.c) \
          $(wildcard $(SRCDIR)/entities/*.c) \
          $(wildcard $(SRCDIR)/hazards/*.c) \
          $(wildcard $(SRCDIR)/levels/*.c) \
          $(wildcard $(SRCDIR)/player/*.c) \
          $(wildcard $(SRCDIR)/screens/*.c) \
          $(wildcard $(SRCDIR)/surfaces/*.c)
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)

.PHONY: all clean run run-debug run-sandbox run-sandbox-debug web

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
ifeq ($(UNAME),Darwin)
	codesign --force --sign - $@
endif

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/collectibles/%.o: $(SRCDIR)/collectibles/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/core/%.o: $(SRCDIR)/core/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/effects/%.o: $(SRCDIR)/effects/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/entities/%.o: $(SRCDIR)/entities/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/hazards/%.o: $(SRCDIR)/hazards/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/levels/%.o: $(SRCDIR)/levels/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/player/%.o: $(SRCDIR)/player/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/screens/%.o: $(SRCDIR)/screens/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

$(SRCDIR)/surfaces/%.o: $(SRCDIR)/surfaces/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -MMD -MP -c -o $@ $<

-include $(DEPS)

SDL_DLL_PATH = /c/msys64/ucrt64/bin

run: all
	PATH="$(SDL_DLL_PATH):$$PATH" ./$(TARGET)

run-debug: all
	PATH="$(SDL_DLL_PATH):$$PATH" ./$(TARGET) --debug

run-sandbox: all
	PATH="$(SDL_DLL_PATH):$$PATH" ./$(TARGET) --sandbox

run-sandbox-debug: all
	./$(TARGET) --sandbox --debug

# ── WebAssembly (Emscripten) ──────────────────────────────────────────
# Requires the Emscripten SDK (emcc on PATH).
# Produces out/super-mango.html, .js, .wasm, and .data (bundled assets).
#
# SDL2 ports are compiled from source by Emscripten on first build;
# subsequent builds reuse the cached port libraries.
WEB_FLAGS = -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' \
            -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 \
            -s SDL2_MIXER_FORMATS='["wav"]' \
            -s ALLOW_MEMORY_GROWTH=1 \
            --preload-file assets --preload-file sounds \
            --shell-file web/shell.html

web: $(OUTDIR)
	emcc -std=c11 -O2 $(SRCS) -o $(OUTDIR)/super-mango.html $(WEB_FLAGS)

clean:
	rm -f $(SRCDIR)/*.o $(SRCDIR)/*.d
	rm -f $(SRCDIR)/collectibles/*.o $(SRCDIR)/collectibles/*.d
	rm -f $(SRCDIR)/core/*.o $(SRCDIR)/core/*.d
	rm -f $(SRCDIR)/effects/*.o $(SRCDIR)/effects/*.d
	rm -f $(SRCDIR)/entities/*.o $(SRCDIR)/entities/*.d
	rm -f $(SRCDIR)/hazards/*.o $(SRCDIR)/hazards/*.d
	rm -f $(SRCDIR)/levels/*.o $(SRCDIR)/levels/*.d
	rm -f $(SRCDIR)/player/*.o $(SRCDIR)/player/*.d
	rm -f $(SRCDIR)/screens/*.o $(SRCDIR)/screens/*.d
	rm -f $(SRCDIR)/surfaces/*.o $(SRCDIR)/surfaces/*.d
	rm -rf $(OUTDIR)
