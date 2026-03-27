# Super Mango

A 2D platformer game written in C using SDL2, built for learning purposes.

## About

Super Mango is an MVP 2D game where a player character moves around an 800×600 window against a sky background. Movement is smooth and frame-rate independent thanks to delta-time physics. The project is intentionally minimal and well-commented so the source code can be read as a learning resource for C + SDL2 game development.

## Prerequisites

You need the following installed on your system:

- **clang** (or any C11-compatible compiler)
- **SDL2** — window, renderer, input, audio
- **SDL2_image** — loading PNG textures
- **SDL2_ttf** — loading TrueType fonts
- **SDL2_mixer** — audio mixing

On macOS with Homebrew:

```sh
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

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
| `W` / `↑` | Move up |
| `S` / `↓` | Move down |
| `A` / `←` | Move left |
| `D` / `→` | Move right |
| `ESC` | Quit |
| Close window | Quit |

Diagonal movement is supported (e.g. `W` + `D` moves up-right).

## Project Structure

```
super-mango-game/
├── Makefile          ← Build system (clang, sdl2-config)
├── assets/           ← PNG sprites and TTF font
│   ├── Player.png
│   ├── Sky_Background_0.png
│   ├── Round9x13.ttf
│   └── ... (more sprites for future use)
└── src/
    ├── main.c        ← Entry point: SDL init/teardown
    ├── game.h        ← GameState struct and constants
    ├── game.c        ← Window, renderer, background, game loop
    ├── player.h      ← Player struct declaration
    └── player.c      ← Player init, input, physics, render
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

Movement uses **delta time** (`dt`): the number of seconds elapsed since the last frame. Multiplying velocity by `dt` makes movement speed consistent regardless of frame rate — so the player moves at 200 px/s whether the game runs at 30 FPS or 120 FPS.

### Frame Cap

VSync is requested from the GPU via `SDL_RENDERER_PRESENTVSYNC`. A manual `SDL_Delay` fallback ensures the loop never spins faster than 60 FPS even if VSync is unavailable.

## Assets

All sprites are PNG files in `assets/`. The project ships with a full set of tiles, enemies, UI elements, and backgrounds ready for future development. `assets/Round9x13.ttf` is available for on-screen text rendering.

## License

MIT — see [LICENSE](LICENSE).
