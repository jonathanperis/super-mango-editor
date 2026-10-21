# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Audit remediation | Shipped | Both audit rounds merged in #306; code-scanning cleanup in #308/#309 (0 open alerts); manual moved to an Astro content collection and re-audited against the code. |
| raylib 6.0 port | Shipped | Game and editor use pinned, checksum-verified raylib 6.0 source (`vendor/raylib/`, `tools/build_raylib.py`) on desktop (bundled GLFW) and Web (Emscripten 6.0.9). SDL2 and its satellite libraries are gone. |
| Fixed-step simulation | Shipped | `src/core/game_timing.c` feeds frame time into an accumulator and advances fixed 1/60 s steps; `make timing-lab` compares variable and fixed steps. |
| Runtime level loader | Shipped | Bare game launch loads the v1 campaign manifest and opens its selector; `--level <path>` / `make run-level LEVEL=...` loads TOML directly. |
| Authored checkpoints | Shipped | Optional `[[checkpoints]]` records provide runtime respawns, editor placement/editing/feedback, serializer support, and test coverage. |
| Level editor | Shipped | Standalone raylib editor builds with `make editor` / runs with `make run-editor`; reads/writes TOML-backed `LevelDef` data and playtests private TOML snapshots. |
| Level validation | Shipped | `tools/validate_levels.py`, `make validate-levels` (CI on Linux, macOS and Windows), and C validation tests. |
| CI gates | Shipped | `-Werror` on every native leg and the Web leg, sanitizers with fuzz-seed replay, scripted replay smoke, Windows-only real-backend desktop job (Mesa llvmpipe, `RAYLIB_AUDIO=null`), Windows Clang job, WebAssembly artifact checks, always-on docs drift job plus docs lint/build, CodeQL (C/C++ SARIF filtered for third-party code-quality noise; Actions, Python, JS/TS), release `SHA256SUMS` and build provenance attestations. |
| Pages publishing | Shipped | `pages-build` → `pages-deploy` jobs at the end of `build.yml` use the same run's WASM artifact; only the deploy job holds Pages/OIDC permissions. |
| Mid-level Continue | Shipped | The profile (format_version 2) keeps one `[resume]` Continue point: respawn point, score, lives, collected coins and timer, bound to the level's content hash. Recorded on a new respawn point, a pause or leaving part-way; cleared on finishing or losing the level; offered by the menu's **Continue** and `--continue` (`src/core/game_resume.c`). |
| GameState parts | Shipped | `GameState` (`src/game.h`) is three by-value parts: `world` (`GameWorld`: the level being played, ~47 KB), `screen` (`GameScreen`: HUD, overlays, routes, timing, input, replay, debug, ~2 KB) and `assets` (`GameAssets`: shared sprites and sounds, loaded once per `AppSession` and lent to every game), plus `cache`, a borrowed pointer to the session's `AssetCache` (`src/shared/asset_cache.h`): a reference-counted cache, keyed by path, of the files a level names and of the player sheet, HUD icon and font. Replay, Retry and re-entering a level read nothing from disk (was 15 textures, the music and the font on `01_lugio_01`); another level reads only the files it adds, and what it does not use is unloaded once it has opened, so the cache holds one level's files (at most 15 textures, 3.1 MiB). The start menu's font, logo and confirm sound are in the same cache and the session holds them between menus, so reopening the menu after Level Select reads nothing (was 1 texture, 1 sound and a font handle each time). Headers that only pass `GameState *` include `src/game_fwd.h`; `levels/level.h` does not include `game.h`, so touching `game.h` recompiles 65 of 231 headless objects (was 158). |
| Entity tables | Shipped | What happens to each entity kind, and when, is four tables of `{ ..., function }` rows, each walked top to bottom by one loop: `s_level_loaders` (`src/levels/level_loader.c`, load order and per-level/per-life), `s_update_steps` (`src/core/game_update.c`, the 20 steps of one fixed step after the player moves, collision included), `s_damage_sources` (`src/collision/game_collision.c`, hit priority) and `s_render_layers` (`src/render/game_render.c`, the 33 render layers). Each row names a small adapter (`update_spiders`, `render_spiders`) that calls the entity module with the inputs it needs; the modules are unchanged. `make docs-drift` checks the update and render tables against the Update Order and Render Order tables in `docs/wiki/architecture.md`. Adding a kind: its module, then a row (and adapter) in each table it takes part in. |
| Time-trial ghost | Shipped | Each normal run records one position/sprite sample per fixed step; a finished run that beats the stored one becomes the level's ghost (TOML next to the profile, or a per-level `localStorage` entry), bound to the level's content hash and capped at five minutes. Later attempts race a translucent ghost; Settings has **Ghost (best run)** on/off (`src/core/game_ghost.c`, `game_ghost_file.c`). |

## Current Repository Reality

- Runtime levels are TOML files in `levels/`:
  - `levels/00_sandbox_01.toml` (Creator's Playground)
  - `levels/01_lugio_01.toml` (Volcanic Depths 1)
  - `levels/02_lugio_02.toml` (Volcanic Depths 2)
  - six learning labs in `levels/labs/` (`01_collision` … `06_camera`)
- Campaign manifest: `levels/campaigns/main.toml` is required for the native selector. Its ordered, unique `levels/*.toml` entries form Creator's Playground → Volcanic Depths 1 → Volcanic Depths 2 and must form a linear `[last_star].next_phase` chain; the final entry has no `next_phase`. The editor's Campaign view (`Ctrl+M`) edits it with the same C rules the start menu applies (`src/levels/campaign_catalog.c`), including relinking every `next_phase` to the order.
- Parser: vendored `vendor/tomlc17/tomlc17.c`.
- Serializer: shared by game and editor in `src/shared/serializer*.c`.
- Editor target is separate: `out/super-mango-editor`, built from `src/editor/` against the same raylib library; native file pickers and confirmations go through `src/editor/file_dialog.c` (Linux uses zenity).
- Text: raylib's built-in default font; no font file ships.
- Audio: twelve WAVs generated by `tools/gen_sounds.py`; `make docs-drift` checks them within 1 LSB.
- Tests: `make test` runs 18 native binaries: level-serializer, level-validate, runtime-load, rail, entity-utils, collision, phase-transition, editor-validation, editor-undo-recovery, gameplay-damage, gameplay-config, gameplay-score, game-overlay, game-events, session, game-checkpoint, gameplay-mechanics, and editor-ui. It also runs a native parser-allocation probe, `tests/validate_levels_test.py`, `tests/gen_sounds_test.py`, `tests/parser_validator_test.py`, and the web-host contract (`tools/check_web_boot_contract.py`, four Node tests: web host, profile storage, touch controls, keyboard scope; `tests/package_release_test.py`; `tests/filter_codeql_sarif_test.py`). `tests/docs_checks_test.py` runs through `bun run check-site`.
- Validation: `make validate-levels` runs `python3 tools/validate_levels.py` against current TOML levels.
- Checkpoints: every record needs finite `x`/`y`; x is unique, strictly after the effective start, and within the world. The furthest crossed record determines `respawn_x`/`respawn_y`; any authored record disables automatic screen-boundary fallback, while no records preserves it. Retry/replay/next level reset to the destination level start.
- Editor safeguards: in-memory validation blocks save/autosave/playtest on errors, status/panel show the validation summary, recovery snapshots and recent files live in the OS preference directory (`Super Mango/Editor/`), and `--smoke-test` supports CI.

## Decisions (Current)

### D-001: TOML Runtime Level Format
TOML is canonical for editable and shipped levels. `LevelDef` remains in-memory schema. `tomlc17` is vendored and used by runtime/editor serialization paths. No C level-export workflow remains.

### D-002: Custom raylib Editor UI
Standalone editor uses a custom immediate-mode UI drawn with raylib and its built-in font. No external UI toolkit. (Replaces the original SDL2 + SDL2_ttf decision.)

### D-003: Direct Level Loading
Bare launch loads the validated v1 `levels/campaigns/main.toml` selector. `--level <path>` / `make run-level LEVEL=...` bypasses it and loads that TOML level directly, even when the path is not in the manifest.

### D-004: Fixed Simulation Step
Gameplay advances in fixed 1/60 s steps from a frame-time accumulator instead of a variable frame `dt`, so physics results do not depend on the display refresh rate.

### D-005: The Site Stays on github.io (superseded 2026-10-21)
The website and the browser game stay at `https://jonathanperis.github.io/super-mango-editor/`; there is no custom domain (decided 2026-10-10). Browser storage is therefore shared with the account's other GitHub Pages sites, which `SECURITY.md` documents as an accepted risk: the game treats every stored value as untrusted input. A custom domain would isolate that storage, but it needs an owned domain and DNS, and players' saved profiles and ghosts are tied to the current address and would not carry over.

**Superseded 2026-10-21:** the owner moved the website and the browser game to `https://super-mango-editor.jonathanperis.tech/` (commit `d9982fe`, `docs/public/CNAME`). The game now has its own origin, so profiles and ghosts in browser storage are no longer shared with the account's other GitHub Pages sites; `SECURITY.md` describes the new scope. Saves made at the old github.io address stay there and do not carry over. D-005 is kept above only as history.

## Current Counts

| Area | Count | Source |
|------|-------|--------|
| Editor placeable types | 31 | `ENT_COUNT` in `src/editor/editor.h` |
| Authored checkpoint capacity | 99 | `MAX_CHECKPOINTS` in `src/game_constants.h` |
| Enemy types | 6 | spider, jumping_spider, bird, faster_bird, fish, faster_fish |
| Hazard types | 7 | axe_trap, circular_saw, spike_row, spike_platform, spike_block, blue_flame, fire_flame |
| Collectible types | 5 | coin, star_yellow, star_green, star_red (one `health_star` module), last_star |
| Surface/climbable types | 9 | platform, float_platform, bridge, bouncepad small/medium/high, vine, ladder, rope |
| Effect systems | 3 | fog, parallax, water |
| Playable TOML level files | 3 | `levels/*.toml` (plus 6 labs in `levels/labs/`) |
| Campaign manifests | 1 | `levels/campaigns/main.toml` (v1) |

## MAX_* Constants Reference

| Constant | Value |
|----------|-------|
| `MAX_FLOOR_GAPS` | 16 |
| `MAX_CHECKPOINTS` | 99 |
| `MAX_RAILS` | 16 |
| `MAX_PLATFORMS` | 32 |
| `MAX_COINS` | 64 |
| `MAX_STAR_YELLOWS` | 16 |
| `MAX_STAR_GREENS` | 16 |
| `MAX_STAR_REDS` | 16 |
| `MAX_SPIDERS` | 16 |
| `MAX_JUMPING_SPIDERS` | 16 |
| `MAX_BIRDS` | 16 |
| `MAX_FASTER_BIRDS` | 16 |
| `MAX_FISH` | 16 |
| `MAX_FASTER_FISH` | 16 |
| `MAX_AXE_TRAPS` | 16 |
| `MAX_CIRCULAR_SAWS` | 16 |
| `MAX_SPIKE_ROWS` | 16 |
| `MAX_SPIKE_PLATFORMS` | 16 |
| `MAX_SPIKE_BLOCKS` | 16 |
| `MAX_BLUE_FLAMES` | 16 |
| `MAX_FIRE_FLAMES` | 16 |
| `MAX_FLOAT_PLATFORMS` | 16 |
| `MAX_BRIDGES` | 16 |
| `MAX_BOUNCEPADS_SMALL` | 16 |
| `MAX_BOUNCEPADS_MEDIUM` | 16 |
| `MAX_BOUNCEPADS_HIGH` | 16 |
| `MAX_VINES` | 24 |
| `MAX_LADDERS` | 16 |
| `MAX_ROPES` | 16 |
| `MAX_BACKGROUND_LAYERS` | 8 |
| `MAX_FOG_TEXTURES` | 4 |

## Deferred Ideas

- Rich editor validation panel: shipped (N-001) — clickable diagnostics, every runtime error listed, and the syntax/schema/runtime problems of a file that fails to load, with lines. Severity beyond error/warning (an "info" class) remains open.
- Metadata-editor ergonomics for shipped background/foreground/fog arrays and physics overrides.
- Serializer round-trip fixture expansion beyond current regression tests.
- Recovery snapshot presentation (a richer picker than native three-button dialogs). Cleanup shipped: Discard in the picker, orphan sweep at start-up, owner-PID guard for live snapshots, and a specific "folder full" status.
- Tilemap painting for custom floor layouts.
