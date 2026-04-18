# ── Compiler + SDL2 detection ────────────────────────────────────────
# Both CC and SDL2CFG can be overridden on the command line:
#   make CC=gcc
#   make CC=gcc SDL2CFG=/custom/sdl2-config
#
# SDL2CFG always resolves from PATH so it works in any MSYS2/UCRT64
# shell (local or CI) without needing a hardcoded install location.
#
# CC defaults to the MSYS2 UCRT64 clang when running on Windows outside
# an MSYS2 shell (e.g. Git Bash), where the wrong clang from Git for
# Windows would otherwise be picked up first.  Inside an MSYS2 shell
# (CI: shell: msys2 {0}, or local: launched from MSYS2 terminal) the
# right gcc/clang is already first on PATH, so the ?= default is ignored
# and only the SDL2CFG path matters — which is just "sdl2-config".

SDL2CFG ?= sdl2-config

ifeq ($(OS),Windows_NT)
CC      ?= /c/msys64/ucrt64/bin/clang.exe
else
CC      ?= clang
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
          $(wildcard $(SRCDIR)/surfaces/*.c) \
          $(SRCDIR)/editor/serializer.c \
          vendor/tomlc17/tomlc17.c
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)

# ── Editor (standalone level editor) ─────────────────────────────────
EDITOR_DIR    = src/editor
VENDOR_DIR    = vendor/tomlc17
EDITOR_SRCS   = $(wildcard $(EDITOR_DIR)/*.c) $(VENDOR_DIR)/tomlc17.c \
                src/surfaces/rail.c
EDITOR_OBJS   = $(EDITOR_SRCS:.c=.o)
EDITOR_DEPS   = $(EDITOR_OBJS:.o=.d)
EDITOR_TARGET = $(OUTDIR)/super-mango-editor
EDITOR_LIBS   = $(shell $(SDL2CFG) --libs) -lSDL2_image -lSDL2_ttf -lm

.PHONY: all clean run run-debug run-level run-level-debug web editor run-editor

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
ifeq ($(OS),Windows_NT)
else ifeq ($(shell uname -s),Darwin)
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

# ── Run targets (cross-platform) ─────────────────────────────────────
# Windows needs SDL DLL path in PATH; Linux/macOS use system libs.

ifeq ($(OS),Windows_NT)
SDL_DLL_PATH ?= /c/msys64/ucrt64/bin
RUN_PREFIX = PATH="$(SDL_DLL_PATH):$$PATH"
else
RUN_PREFIX =
endif

run: all
	$(RUN_PREFIX) ./$(TARGET)

run-debug: all
	$(RUN_PREFIX) ./$(TARGET) --debug

run-level: all
	$(RUN_PREFIX) ./$(TARGET) --level $(LEVEL)

run-level-debug: all
	$(RUN_PREFIX) ./$(TARGET) --debug --level $(LEVEL)

# ── Editor targets ───────────────────────────────────────────────────
editor: $(OUTDIR) $(EDITOR_TARGET)

$(EDITOR_TARGET): $(EDITOR_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(EDITOR_LIBS)
ifeq ($(OS),Windows_NT)
else ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - $@
endif

$(EDITOR_DIR)/%.o: $(EDITOR_DIR)/%.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(VENDOR_DIR) -MMD -MP -c -o $@ $<

$(VENDOR_DIR)/%.o: $(VENDOR_DIR)/%.c
	$(CC) -std=c11 -MMD -MP -c -o $@ $<

run-editor: editor
	./$(EDITOR_TARGET)

-include $(EDITOR_DEPS)

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
            --preload-file assets \
            --preload-file levels \
            --shell-file web/shell.html

web: $(OUTDIR)
	emcc -std=c11 -O2 -I$(SRCDIR) $(SRCS) -o $(OUTDIR)/super-mango.html $(WEB_FLAGS)
	emcc -std=c11 -O2 -I$(SRCDIR) $(SRCS) -o $(OUTDIR)/super-mango-debug.html $(WEB_FLAGS) \
		-s INVOKE_RUN=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["callMain"]' \
		--post-js web/debug-boot.js

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
	rm -f $(EDITOR_DIR)/*.o $(EDITOR_DIR)/*.d
	rm -f $(VENDOR_DIR)/*.o $(VENDOR_DIR)/*.d
	rm -rf $(OUTDIR)
