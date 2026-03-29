# Build System

← [Home](Home)

---

## Makefile Overview

The project uses a **GNU Makefile** that auto-discovers source files via a wildcard — no manual edits required when adding new `.c` files.

```makefile
CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags)
LIBS    = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer
OUTDIR  = out
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c)
OBJS    = $(SRCS:.c=.o)
```

### Key Variables

| Variable | Value | Description |
|----------|-------|-------------|
| `CC` | `clang` | C compiler (override with `CC=gcc` if needed) |
| `CFLAGS` | see below | Compiler flags |
| `LIBS` | see below | Linker flags |
| `TARGET` | `out/super-mango` | Output binary path |
| `SRCS` | `src/*.c` | All C source files (auto-discovered) |
| `OBJS` | `src/*.o` | Object files, placed next to sources |

### Compiler Flags Explained

| Flag | Meaning |
|------|---------|
| `-std=c11` | Compile as C11 (ISO/IEC 9899:2011) |
| `-Wall` | Enable common warnings |
| `-Wextra` | Enable extra warnings beyond `-Wall` |
| `-Wpedantic` | Strict ISO compliance warnings |
| `$(shell sdl2-config --cflags)` | SDL2 include paths (`-I/opt/homebrew/include/SDL2`) |

### Linker Flags Explained

| Flag | Meaning |
|------|---------|
| `$(shell sdl2-config --libs)` | SDL2 core library (`-L/opt/homebrew/lib -lSDL2`) |
| `-lSDL2_image` | PNG/JPG texture loading |
| `-lSDL2_ttf` | TrueType font rendering |
| `-lSDL2_mixer` | Audio mixing (WAV, MP3, OGG) |

---

## Build Targets

### `make` / `make all`

Compiles all `src/*.c` files to `.o` objects, then links them into `out/super-mango`.

```sh
make
```

**Steps:**
1. Creates `out/` directory if it does not exist
2. Compiles each `src/*.c` → `src/*.o`
3. Links all `.o` files → `out/super-mango`
4. Ad-hoc code signs the binary with `codesign --force --sign - $@` (required on macOS Apple Silicon to avoid `Killed: 9` errors)

### `make run`

Builds (if out of date) then immediately executes the binary (no CLI flags).

```sh
make run
```

The binary must be run from the **repo root** because asset paths are relative:
```c
IMG_LoadTexture(renderer, "assets/parallax/sky.png");
Mix_LoadWAV("sounds/jump.wav");
```

### `make run-debug`

Builds (if out of date) then runs the binary with the `--debug` flag, which enables the debug overlay: FPS counter, collision hitbox visualization, and scrolling event log.

```sh
make run-debug
```

### `make clean`

Removes all build artifacts.

```sh
make clean
```

Deletes:
- `src/*.o` — all object files
- `out/` — the output directory and binary

---

## Prerequisites

### macOS (Apple Silicon / Intel)

```sh
# Install Homebrew if needed: https://brew.sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Xcode Command Line Tools (provides clang and make)
xcode-select --install
```

SDL2 libraries are installed to `/opt/homebrew/` on Apple Silicon. `sdl2-config` resolves the correct paths automatically.

### Linux — Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential clang \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

### Linux — Fedora / RHEL / CentOS

```sh
sudo dnf install clang make \
    SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel
```

### Linux — Arch Linux

```sh
sudo pacman -S clang make sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

### Windows (MSYS2)

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal:

```sh
pacman -S mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_image \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-SDL2_mixer
```

3. Build:

```sh
cd /c/path/to/super-mango-game
make
```

4. SDL2 DLLs must be in the same directory as the binary. Copy them from the MSYS2 prefix.

---

## Adding New Source Files

Because the Makefile uses `$(wildcard src/*.c)`, any new `.c` file placed in `src/` is compiled automatically on the next `make` invocation. No Makefile changes required.

```sh
# Example: adding a coin entity
touch src/coin.c src/coin.h
make   # coin.c is compiled automatically
```

See [Developer Guide](Developer-Guide) for the full new-entity workflow.

---

## Output Structure

After a successful build:

```
out/
└── super-mango     ← the game binary
src/
├── main.o
├── game.o
├── player.o
├── platform.o
├── water.o
├── spider.o
├── fog.o
├── parallax.o
├── coin.o
├── vine.o
├── fish.o
├── hud.o
├── bouncepad.o
├── rail.o
├── spike_block.o
└── debug.o
```
