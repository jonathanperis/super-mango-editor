# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Audit remediation | Shipped | Both audit rounds merged in #306; code-scanning cleanup in #308/#309 (0 open alerts); manual moved to an Astro content collection and re-audited against the code. |
| raylib 6.2 port | Shipped | Game and editor use pinned, checksum-verified raylib 6.2 source (`vendor/raylib/manifest.json`, `tools/build_raylib.py`) on desktop (bundled GLFW) and Web (the Makefile's `EMSCRIPTEN_VERSION`). SDL2 and its satellite libraries are gone. |
| Fixed-step simulation | Shipped | `src/core/game_timing.c` feeds frame time into an accumulator and advances fixed 1/60 s steps; `make timing-lab` compares variable and fixed steps. |
| Runtime level loader | Shipped | Bare game launch loads the v1 campaign manifest and opens its selector; `--level <path>` / `make run-level LEVEL=...` loads TOML directly. |
| Authored checkpoints | Shipped | Optional `[[checkpoints]]` records provide runtime respawns, editor placement/editing/feedback, serializer support, and test coverage. |
| Level editor | Shipped | Standalone raylib editor builds with `make editor` / runs with `make run-editor`; reads/writes TOML-backed `LevelDef` data and playtests private TOML snapshots. |
| Level validation | Shipped | `tools/validate_levels.py`, `make validate-levels` (CI on Linux, macOS and Windows), and C validation tests. |
| CI gates | Shipped | `-Werror` on every native leg and the Web leg, sanitizers with fuzz-seed replay, scripted replay smoke, Windows-only real-backend desktop job (Mesa llvmpipe, `RAYLIB_AUDIO=null`), Windows Clang job, WebAssembly artifact checks, always-on docs drift job plus docs lint/build, CodeQL (C/C++ SARIF filtered for third-party code-quality noise; Actions, Python, JS/TS), release `SHA256SUMS` and build provenance attestations. |
| Pages publishing | Shipped | `pages-build` → `pages-deploy` jobs at the end of `build.yml` use the same run's WASM artifact; only the deploy job holds Pages/OIDC permissions. |
| Mid-level Continue | Shipped | The profile (format_version 2) keeps one `[resume]` Continue point: respawn point, score, lives, collected coins and timer, bound to the level's content hash. Recorded on a new respawn point, a pause or leaving part-way; cleared on finishing or losing the level; offered by the menu's **Continue** and `--continue` (`src/core/game_resume.c`). |
| GameState parts | Shipped | `GameState` (`src/game.h`) is three by-value parts: `world` (`GameWorld`: the level being played, ~47 KB), `screen` (`GameScreen`: HUD, overlays, routes, timing, input, replay, debug, ~2 KB) and `assets` (`GameAssets`: shared sprites and sounds, loaded once per `AppSession` and lent to every game), plus `cache`, a borrowed pointer to the session's `AssetCache` (`src/shared/asset_cache.h`): a reference-counted cache, keyed by path, of the files a level names and of the player sheet, HUD icon and font. Replay, Retry and re-entering a level read nothing from disk (was 15 textures, the music and the font on `01_lugio_01`); another level reads only the files it adds, and what it does not use is unloaded once it has opened, so the cache holds one level's files (at most 15 textures, 3.1 MiB). The start menu's font, logo and confirm sound are in the same cache and the session holds them between menus, so reopening the menu after Level Select reads nothing (was 1 texture, 1 sound and a font handle each time). Headers that only pass `GameState *` include `src/game_fwd.h`; `levels/level.h` does not include `game.h`, so touching `game.h` recompiles 65 of 231 headless objects (was 158). |
| Entity tables | Shipped | What happens to each entity kind, and when, is tables of `{ ..., function }` rows, each walked top to bottom by one loop (a fifth, `s_movers` in `src/core/game_movers.c`, lists what is drawn between two steps, and `s_damage_sources` rows now give a `hitbox` function the debug overlay draws from too): `s_level_loaders` (`src/levels/level_loader.c`, load order and per-level/per-life), `s_update_steps` (`src/core/game_update.c`, the 20 steps of one fixed step after the player moves, collision included), `s_damage_sources` (`src/collision/game_collision.c`, hit priority) and `s_render_layers` (`src/render/game_render.c`, the 33 render layers). Each row names a small adapter (`update_spiders`, `render_spiders`) that calls the entity module with the inputs it needs; the modules are unchanged. `make docs-drift` checks the update and render tables against the Update Order and Render Order tables in `docs/wiki/architecture.md`. Adding a kind: its module, then a row (and adapter) in each table it takes part in. |
| Audit round 3 | In progress (uncommitted, branch `feat/audit-remediation-round-3`) | Runtime: a bouncepad launch and a knockback ignore jump input (`jump_cut_armed`, `player_cancel_jump_state`); waiting spike blocks start from the player's position, not the settings-dependent camera (`game_camera_reveal_right`); Replay of a level that no longer loads opens the menu; a health star stays at full hearts; the input release latch is per device; experiment captures are `format_version = 4`. Input: level, campaign and experiment files are read once with a size limit (`serializer_read_bytes`, `level_load_toml_file`), hashed and parsed from that one buffer; `CAMPAIGN_MAX_LEVELS` is 128; asset paths follow the portable-name rule; files without `format_version` report ignored keys; a fourth fuzz harness covers the campaign and experiment loaders. Editor: in-window path prompt when no file picker exists, a persistent recovery count in the status bar, a recovery-specific save prompt, absolute document paths, dropdowns that flip upward, platform tiles previewed by path, undo history kept across Campaign Save. Performance: a checkpoint's profile write waits for a pause; the native frame cap follows the display; autosave reads the recovery folder once. Structure: one schema table (key, type, storage offset, default, omit rule) drives the loader and writer of every level value: level-wide keys, `[physics]`, `[last_star]`, `floor_gaps`, `[[checkpoints]]` and every `[[array]]` (`serializer_load_schema`, `serializer_write_entry`; only `format_version` is read and written by hand), with a Python parity test; a movers table for between-step drawing (`src/core/game_movers.c`); one hitbox function per damage source, shared with the debug overlay; `PlayerSurfaces` / `PlayerLanding`; separate gameplay and cosmetic random streams; property rows per entity type with shared-field editing for a multi-selection. Editor features: patrol ranges and resize handles on the canvas (`src/editor/editor_handles.c`), an Entities list with search (`Ctrl+F`, `src/editor/editor_entity_list.c`), per-row layer reordering. Build and CI: local composite actions, shared raylib cache key, `gh release create`, one test list in the Makefile, one web boot script (`web/boot.js`) for both hosts. Verified locally, not yet on GitHub Actions: the clang gates on macOS, and on Linux (Docker, ubuntu:24.04) `make test` and `make release` with GCC `-Werror`, `make smoke` on the desktop backend under Xvfb, the docs site with bun 1.4.2, and `actionlint`; `tools/coverage_baseline.txt` regenerated on Linux with every file within 2 points of the old numbers. Windows and a real macOS window are unverified. |
| Time-trial ghost | Shipped | Each normal run records one position/sprite sample per fixed step; a finished run that beats the stored one becomes the level's ghost (TOML next to the profile, or a per-level `localStorage` entry), bound to the level's content hash and capped at five minutes. Later attempts race a translucent ghost; Settings has **Ghost (best run)** on/off (`src/core/game_ghost.c`, `game_ghost_file.c`). |

## Current Repository Reality

- Runtime levels are TOML files in `levels/`:
  - `levels/00_sandbox_01.toml` (Creator's Playground)
  - `levels/01_lugio_01.toml` (Volcanic Depths 1)
  - `levels/02_lugio_02.toml` (Volcanic Depths 2)
  - six learning labs in `levels/labs/` (`01_collision` … `06_camera`)
- Campaign manifest: `levels/campaigns/main.toml` is required for the native selector. Its ordered, unique `levels/*.toml` entries form Creator's Playground → Volcanic Depths 1 → Volcanic Depths 2 and must form a linear `[last_star].next_phase` chain; the final entry has no `next_phase`. The editor's Campaign view (`Ctrl+M`) edits it with the same C rules the start menu applies (`src/levels/campaign_catalog.c`), including relinking every `next_phase` to the order.
- Parser: vendored `vendor/tomlc17/tomlc17.c`.
- Serializer: shared by game and editor in `src/shared/serializer*.c`. The schema table in `serializer_parse.c` names every key, its type, the struct member that holds it and its default; one loader and one writer walk it for every `[[array]]` of tables. `level-check --dump-schema` prints it and `tests/schema_parity_test.py` (run by `make validate-levels`) compares `tools/validate_levels.py` with it.
- Editor target is separate: `out/super-mango-editor`, built from `src/editor/` against the same raylib library; native file pickers and confirmations go through `src/editor/file_dialog.c` (Linux uses zenity).
- Text: raylib's built-in default font; no font file ships.
- Audio: twelve QOA files generated by `tools/gen_sounds.py`; `make docs-drift` checks them within 1 LSB.
- Tests: `make test` runs 18 native binaries: level-serializer, level-validate, runtime-load, rail, entity-utils, collision, phase-transition, editor-validation, editor-undo-recovery, gameplay-damage, gameplay-config, gameplay-score, game-overlay, game-events, session, game-checkpoint, gameplay-mechanics, and editor-ui. It also runs a native parser-allocation probe; these Python tests of the build and docs tools: `tests/validate_levels_test.py`, `tests/gen_sounds_test.py`, `tests/fuzz_dict_test.py`, `tests/coverage_baseline_test.py`, `tests/clang_tidy_report_test.py`, `tests/doctor_test.py`, `tests/build_raylib_test.py`, `tests/gen_compile_commands_test.py`, `tests/roadmap_quality_test.py`, `tests/import_raygui_icons_test.py` and `tests/parser_validator_test.py`; and the web-host contract: `tools/check_web_boot_contract.py`, six Node tests (`tests/boot_test.cjs` for the shared boot script, `tests/web_host_test.cjs`, `tests/profile_storage_test.cjs`, `tests/touch_controls_test.cjs`, `tests/keyboard_scope_test.cjs`, `tests/audio_unlock_test.cjs`) and `tests/web_boot_contract_test.py`, `tests/web_shell_test.py`, `tests/web_debug_page_test.py`, `tests/wasm_artifacts_test.py`, `tests/package_release_test.py`, `tests/filter_codeql_sarif_test.py`. `tests/docs_checks_test.py` runs through `bun run check-site`, and `make validate-levels` runs `tests/schema_parity_test.py` (the Python copy of the level schema against the C table). `make docs-drift` checks this list against the Makefile and the files in `tests/`.
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
- Metadata-editor ergonomics for physics overrides (per-row move/remove for background, foreground and fog layers shipped).
- Serializer round-trip fixture expansion beyond current regression tests.
- Recovery snapshot presentation (a richer picker than the three-button prompt). Cleanup shipped: Discard in the picker, orphan sweep at start-up (snapshots without metadata, metadata without snapshots, playtest files of editors no longer running; a snapshot with damaged metadata gets new metadata), owner-PID guard for live snapshots, the open document's own snapshot left out of the picker, a persistent count in the status bar, and a specific "folder full" status.
- Tilemap painting for custom floor layouts.
