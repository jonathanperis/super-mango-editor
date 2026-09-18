# Super Mango Editor

> Play a C11/raylib platformer, build TOML worlds, and learn how the engine works.

Super Mango is a 2D platformer built in C11 with raylib, designed as an educational project for learning game development. The game features dynamic multi-screen TOML worlds with parallax backgrounds, enemies, hazards, collectibles, and delta-time physics, building natively on macOS/Linux/Windows and as WebAssembly for browser play.

---

## Quick Links

### Learn by Doing

| Page | Description |
|------|-------------|
| [Sandbox School](learning-path/) | Eight guided labs, from first frame to reproducible experiments |
| [Mechanics Museum](mechanics-museum/) | Six standalone levels for focused inspection |
| [Entity Walkthrough](entity-walkthrough/) | Trace a collectible through file format, runtime and editor |

### Engine & Code

| Page | Description |
|------|-------------|
| [Architecture](architecture/) | Game loop, init/loop/cleanup pattern, GameState container, 32-layer render order |
| [Controls & Input](controls/) | Keyboard, gamepad, browser/WASM, replay, smoke, and runtime flag reference |
| [Testing & Smoke Matrix](testing/) | Which local/CI checks to run for runtime, editor, docs, WASM, and release changes |
| [Source Files](source-files/) | Module-by-module reference for every `.c` / `.h` file |
| [Player Module](player-module/) | Input, physics, animation and lifecycle across `src/player/` |
| [Constants Reference](constants-reference/) | Curated gameplay constants and runtime-width distinctions |

### Content & Assets

| Page | Description |
|------|-------------|
| [Entities & Hazards](entities-and-hazards/) | All 6 enemy types and 7 hazard types: behaviour, constants, TOML placement |
| [Collectibles & Surfaces](collectibles-and-surfaces/) | Coins, stars, bouncepads, rails, float platforms, climbable surfaces |
| [Assets](assets/) | All sprite sheets, tilesets, and fonts in `assets/` |
| [Sounds](sounds/) | All audio files in `assets/sounds/` |
| [Asset Inventory](asset-inventory/) | Generated raw asset sizes and budget |
| [Asset Provenance](asset-provenance/) | Third-party notices and unresolved media license records |
| [Level Catalog](level-catalog/) | Generated inventory of every stage selected by the v1 campaign manifest, its progression link, and content count |
| [Overlay Snapshots](overlay-snapshots/) | Generated text snapshots for pause and terminal overlays |

### Building & Contributing

| Page | Description |
|------|-------------|
| [Build System](build-system/) | Makefile, compiler flags, build targets, prerequisites for all platforms |
| [Level Design — TOML Reference](level-design/) | Full TOML schema for every entity type; minimum level template |
| [Level Editor](level-editor/) | Visual editor: canvas, palette, properties, undo, play-test, file I/O |
| [Developer Guide](developer-guide/) | Coding conventions, adding new entities, sound effects workflow |
| [Release Checklist](release-checklist/) | Source, docs, WebAssembly, archive, CI, and Pages gates before shipping |

---

## Key Features

- 2D side-scrolling platformer with dynamic multi-screen worlds (configurable via `screen_count`)
- 32 render layers drawn back-to-front with per-level configurable parallax backgrounds
- Delta-time physics with timestep-dependent numerical tradeoffs; compare rates with `make timing-lab`
- Six enemy types (spider, jumping spider, bird, faster bird, fish, faster fish)
- Seven hazard types (spike, spike block, spike platform, circular saw, axe trap, blue flame, fire flame)
- Five collectible types (coin, star yellow/green/red, last star)
- Climbable vines, ladders, ropes; three bouncepad tiers (small/medium/high); crumble bridges; float platforms (static/crumble/rail-riding)
- TOML-only level workflow: `levels/campaigns/main.toml` orders the sandbox and two volcanic stages; `--level <path>` also opens the separate learning labs
- Authored `[[checkpoints]]` supply explicit respawns; levels without records retain automatic screen-boundary respawns
- Pause, game-over, and level-completion overlays: terminal menus support Next Level, Replay, Level Select, Exit, or Retry as applicable; Up/Down or D-pad selects, Enter/Space/Start confirms (A also confirms), Esc/Back exits (B also exits)
- Standalone visual level editor with undo, copy/paste, validation blocking, recent files, autosave, and play-test integration
- Start menu, HUD, lives, F1 settings, saved preferences and per-level best results
- Opt-in debug inspector: freeze/step, slow motion, live tuning and experiment capture/replay
- Keyboard, hot-plug gamepad and browser touch controls
- Builds natively on macOS, Linux, Windows; WebAssembly via Emscripten

**[Play in browser →](https://jonathanperis.github.io/super-mango-editor/)**

---

## Project at a Glance

| Item | Detail |
|------|--------|
| Language | C11 |
| Compiler | `clang` recommended for CI/local parity; `gcc` compatible |
| Window size | 800 × 600 px (OS window) |
| Logical canvas | 400 × 300 px (2× pixel scale) |
| Target FPS | 60 |
| Audio | raylib/miniaudio-negotiated device format; decoded effects and streamed WAV music |
| Libraries | raylib 6.0 (bundled GLFW on desktop), tomlc17 (TOML parser) |
| Level format | TOML (`.toml` files in `levels/`) |

---

## Quick Start

```sh
# macOS — install dependencies
brew install cmake python

# Build and run the game
make run CC=clang

# Build and run the level editor
make run-editor CC=clang

# Run a specific level file
make run-level CC=clang LEVEL=levels/labs/01_collision.toml

# Optional local WebAssembly preflight (CI is authoritative for WASM releases)
make web
```

See [Build System](build-system/) for Linux and Windows instructions.
