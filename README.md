# Super Mango

A 2D platformer game written in C using SDL2, built for learning purposes.

## About

Super Mango is a 2D platformer where a player character runs and jumps across a forest stage with a grass floor. The game renders at a 400×300 logical resolution scaled 2× to an 800×600 OS window, giving a chunky pixel-art look. Movement is smooth and frame-rate independent thanks to delta-time physics. The project is intentionally minimal and well-commented so the source code can be read as a learning resource for C + SDL2 game development.

## Prerequisites

You need the following installed on your system:

- **clang** or **gcc** (any C11-compatible compiler)
- **make**
- **SDL2** — window, renderer, input, audio
- **SDL2_image** — loading PNG textures
- **SDL2_ttf** — loading TrueType fonts
- **SDL2_mixer** — audio mixing

### macOS

Install via [Homebrew](https://brew.sh):

```sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

The compiler (`clang`) and `make` are included with Xcode Command Line Tools:

```sh
xcode-select --install
```

### Linux (Debian / Ubuntu)

```sh
sudo apt update
sudo apt install build-essential clang \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

On **Fedora / RHEL / CentOS**:

```sh
sudo dnf install clang make \
    SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel
```

On **Arch Linux**:

```sh
sudo pacman -S clang make sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

### Windows

The recommended approach on Windows is to use **MSYS2**, which gives you a Unix-like shell, `make`, and the MinGW-w64 compiler toolchain.

1. Download and install [MSYS2](https://www.msys2.org/).

2. Open the **MSYS2 UCRT64** terminal and install the dependencies:

```sh
pacman -S mingw-w64-ucrt-x86_64-clang \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_image \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-SDL2_mixer
```

3. In the same UCRT64 terminal, navigate to the project and build:

```sh
cd /c/path/to/super-mango-game
make
```

4. The SDL2 DLLs must be in the same directory as the binary to run it. Copy them from the MSYS2 prefix:

```sh
cp /ucrt64/bin/SDL2*.dll out/
```

Then run `./out/super-mango.exe` from the terminal, or double-click `out/super-mango.exe` in Explorer.

## Building

```sh
# Build the binary into out/
make

# Build and run immediately
make run

# Remove all build artifacts
make clean
```

The compiled binary is placed at `out/super-mango`.

## Controls

| Key | Action |
|---|---|
| `Space` | Jump |
| `A` / `←` | Move left |
| `D` / `→` | Move right |
| `ESC` | Quit |
| Close window | Quit |

## Project Structure

```
super-mango-game/
├── Makefile               ← Build system (clang, sdl2-config)
├── assets/                ← PNG sprites and TTF font
│   ├── Player.png
│   ├── Forest_Background_0.png
│   ├── Grass_Tileset.png
│   ├── Round9x13.ttf
│   └── ... (more sprites for future use)
├── sounds/                ← WAV/MP3 sound effects
│   ├── jump.wav
│   ├── water-ambience.ogg
│   └── ... (more sounds for future use)
└── src/
    ├── main.c             ← Entry point: SDL init/teardown
    ├── game.h             ← GameState struct and constants
    ├── game.c             ← Window, renderer, textures, sound, game loop
    ├── player.h           ← Player struct declaration
    └── player.c           ← Player init, input, physics, render
```

## Architecture

The game follows a simple **init → loop → cleanup** pattern:

```
main()
  └── SDL/audio/image/font init
       └── game_init()      — create window, renderer, load textures
            └── game_loop() — event poll → update → render (60 FPS)
                 └── game_cleanup() — destroy all resources in reverse order
  └── SDL subsystem teardown
```

### Delta Time

Movement uses **delta time** (`dt`): the number of seconds elapsed since the last frame. Multiplying velocity by `dt` makes movement speed consistent regardless of frame rate — so the player moves at 160 px/s whether the game runs at 30 FPS or 120 FPS.

### Frame Cap

VSync is requested from the GPU via `SDL_RENDERER_PRESENTVSYNC`. A manual `SDL_Delay` fallback ensures the loop never spins faster than 60 FPS even if VSync is unavailable.

## Assets

All sprites are PNG files in `assets/`. The project ships with a full set of tiles, enemies, UI elements, and backgrounds ready for future development. `assets/Round9x13.ttf` is available for on-screen text rendering.

## Credits

### Art Assets

All sprites, tilesets, and backgrounds are from the **Super Mango 2D Pixel Art Platformer Asset Pack (16×16)** by **Juho** on itch.io:

- [https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16](https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16)

### Game Reference

This game is based on the Mario-style platformer by **JSLegendDev**, built with Kaboom.js. The game design, mechanics, and sound effects were originally created for that project:

- [https://github.com/JSLegendDev/Mario-Game-Kaboom.js](https://github.com/JSLegendDev/Mario-Game-Kaboom.js)

## License

MIT — see [LICENSE](LICENSE).
