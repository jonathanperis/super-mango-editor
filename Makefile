CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags)
LIBS    = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
OUTDIR  = out
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c)
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)

.PHONY: all clean run run-debug run-sandbox run-sandbox-debug web

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

UNAME := $(shell uname -s)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
ifeq ($(UNAME),Darwin)
	codesign --force --sign - $@
endif

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

run: all
	./$(TARGET)

run-debug: all
	./$(TARGET) --debug

run-sandbox: all
	./$(TARGET) --sandbox

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
	rm -f $(SRCDIR)/*.o
	rm -f $(SRCDIR)/*.d
	rm -rf $(OUTDIR)
