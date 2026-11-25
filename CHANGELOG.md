# Changelog

The larger pieces of work on Super Mango, newest first. Each entry is the
note I kept in `.specs/project/STATE.md` while the work was open; that file
now keeps a one-line status per item and links here. Entries are history:
they say what was true when the work merged, and the code and the
[manual](docs/wiki/) say what is true now. Dates are merge dates where I
have them.

## Round 5 enhancements

Merged 2026-11-13, PR #477.

Live site check: after Pages deploy, Build & Release loads the published game in headless Chromium (`tools/live-check/check_live_site.mjs`, Playwright pinned in `tools/live-check/package.json`), waits up to five minutes for the page to link the commit's assets, and fails on a canvas that never reaches its running label, a visible loading line, any console error, page error or failed request, or a game that stops while ArrowRight is held; three known messages (audio waiting for a gesture, `ScriptProcessorNode` deprecation, SwiftShader ReadPixels stall) are allowed by exact text; screenshots go to the `live-site-check` artifact; not a required check. If a newer `main` run deploys while the check waits, the script exits 3 and the job passes only when the compare API says the served commit is newer. CodeQL #194: `placement_same` in `src/editor/snapshot_equal.c` documents its contract and each group of cases. Release checklist section 7 lists the hands-on Windows and macOS window checks CI cannot run. Web audio stays on `ScriptProcessorNode`: miniaudio's AudioWorklet build needs `SharedArrayBuffer`, which needs COOP/COEP headers that GitHub Pages cannot send; the options (keep it, a header-adding service worker, or a host that sends headers) are in the build-system manual, "Web audio", and none is taken without the owner's decision. Verified: the live check passes on the published site and fails (exit 1) on a wrong expected version and on a missing page. Windows and a real macOS window are unverified.

## Audit round 4

Merged 2026-11-12, PRs #474 and #475.

Editor: a value typed into a shared property row reaches every selected entity however the field is left (Tab, a click, `Ctrl+S`, `F5`, Undo), not only `Enter`; `Shift+click` on a full 64-entity selection says so instead of adding nothing silently; the Entities list's Find reads `#3` as an index; a nudge right after Undo starts a new step; a Level Config undo keeps the `name` and `next_phase` a Campaign Save wrote; resize handles find their limit by binary search; the in-window path prompt also replaces a failing file picker; closing the window during a prompt cancels it and then quits the usual way; Save refuses a document path with control characters. Legacy loading and diagnostics: files without `format_version` report their warnings even when the load fails; unknown enum names warn and use the row default; non-table items in table arrays are skipped and not counted; bad `floor_gaps` and `[[checkpoints]]` entries are errors with the full TOML path and line; a missing `name` loads empty and saves as `"Untitled"` (`LEVEL_UNTITLED_NAME`); quoted keys and values escape control bytes (`serializer_message_text`). Guards: a `[physics]` override is exactly -1 (`LEVEL_PHYSICS_ENGINE_DEFAULT`) or 0 to its cap (`MAX_PLAYER_SPEED` 960 px/s for top speeds, `MAX_PLAYER_ACCEL` for accelerations), in C, `tools/validate_levels.py`, experiment captures and the live inspector; compile-time member type checks on every schema row; a test that every schema row is saved once. Runtime: the frame cap follows the window to another display (`display_follow_monitor`); saw and spike-block spin is blended between steps; the level timer counts steps (`level_steps`, `game_level_seconds`); a jump held through a death does not jump on respawn; F9 exports go to the player data folder, only below a folder path without control characters (`path_copy_checked`), and a refused folder logs a clear warning instead of an unset path; `-ffp-contract=off` and `game_sinf`/`game_cosf` (`src/core/game_trig.h`) so captures replay across platforms; the debug overlay draws the real coin hitbox and bouncepad trigger strip; one new-enemy checklist above `s_update_steps`. Renovate and drift: Node and raylib version mentions are checked and grouped, frontend versions are no longer quoted, and the docs checkers have unit tests (`tests/docs_drift_unit_test.py`, run by `make docs-drift`). CI layout and caches: documentation-only pull requests skip the C build work (`Changed paths`); one `Windows x86_64 extras` job replaces the Windows Clang and Desktop backend jobs; release freshness is a step of `Docs drift`; one cross-OS raylib source cache, cached release trees, MSYS2 package cache on `main` only; fuzz corpora minimised (`make fuzz-merge`) and kept as an artifact. Hygiene: every level TOML (subfolders included) and the raylib manifest are LF in `.gitattributes`. Verified locally before the PR: on macOS `make test` (with `-Werror`), `sanitize`, `docs-drift`, `validate-levels` and the web build with `-Werror` plus `check_wasm_artifacts.py`; on Linux (Docker, ubuntu:24.04) GCC `-Werror` `make test` and `make release`, `make smoke` under Xvfb, coverage within 2 points of every baseline row (94.30% of lines), the docs site with bun 1.4.4, `actionlint`, and the Renovate validator; simulated Node 26.13.0 and TypeScript 7 updates pass `make docs-drift`. Windows and a real macOS window are unverified.

## Audit round 3

Merged 2026-11-12, PR #473.

Runtime: a bouncepad launch and a knockback ignore jump input (`jump_cut_armed`, `player_cancel_jump_state`); waiting spike blocks start from the player's position, not the settings-dependent camera (`game_camera_reveal_right`); Replay of a level that no longer loads opens the menu; a health star stays at full hearts; the input release latch is per device; experiment captures are `format_version = 4`. Input: level, campaign and experiment files are read once with a size limit (`serializer_read_bytes`, `level_load_toml_file`), hashed and parsed from that one buffer; `CAMPAIGN_MAX_LEVELS` is 128; asset paths follow the portable-name rule; files without `format_version` report ignored keys; a fourth fuzz harness covers the campaign and experiment loaders. Editor: in-window path prompt when no file picker exists, a persistent recovery count in the status bar, a recovery-specific save prompt, absolute document paths, dropdowns that flip upward, platform tiles previewed by path, undo history kept across Campaign Save. Performance: a checkpoint's profile write waits for a pause; the native frame cap follows the display; autosave reads the recovery folder once. Structure: one schema table (key, type, storage offset, default, omit rule) drives the loader and writer of every level value: level-wide keys, `[physics]`, `[last_star]`, `floor_gaps`, `[[checkpoints]]` and every `[[array]]` (`serializer_load_schema`, `serializer_write_entry`; only `format_version` is read and written by hand), with a Python parity test; a movers table for between-step drawing (`src/core/game_movers.c`); one hitbox function per damage source, shared with the debug overlay; `PlayerSurfaces` / `PlayerLanding`; separate gameplay and cosmetic random streams; property rows per entity type with shared-field editing for a multi-selection. Editor features: patrol ranges and resize handles on the canvas (`src/editor/editor_handles.c`), an Entities list with search (`Ctrl+F`, `src/editor/editor_entity_list.c`), per-row layer reordering. Build and CI: local composite actions, shared raylib cache key, `gh release create`, one test list in the Makefile, one web boot script (`web/boot.js`) for both hosts. Verified locally before the merge, then on GitHub Actions (every build, sanitizer, coverage, WebAssembly and Pages job): the clang gates on macOS, and on Linux (Docker, ubuntu:24.04) `make test` and `make release` with GCC `-Werror`, `make smoke` on the desktop backend under Xvfb, the docs site with bun 1.4.2, and `actionlint`; `tools/coverage_baseline.txt` regenerated on Linux with every file within 2 points of the old numbers. A real macOS window is unverified.

## Entity tables

On `main` since 2026-10-10.

What happens to each entity kind, and when, is tables of `{ ..., function }` rows, each walked top to bottom by one loop (a fifth, `s_movers` in `src/core/game_movers.c`, lists what is drawn between two steps, and `s_damage_sources` rows now give a `hitbox` function the debug overlay draws from too): `s_level_loaders` (`src/levels/level_loader.c`, load order and per-level/per-life), `s_update_steps` (`src/core/game_update.c`, the 20 steps of one fixed step after the player moves, collision included), `s_damage_sources` (`src/collision/game_collision.c`, hit priority) and `s_render_layers` (`src/render/game_render.c`, the 33 render layers). Each row names a small adapter (`update_spiders`, `render_spiders`) that calls the entity module with the inputs it needs; the modules are unchanged. `make docs-drift` checks the update and render tables against the Update Order and Render Order tables in `docs/wiki/architecture.md`. Adding a kind: its module, then a row (and adapter) in each table it takes part in.

## GameState parts

On `main` since 2026-10-09.

`GameState` (`src/game.h`) is three by-value parts: `world` (`GameWorld`: the level being played, ~47 KB), `screen` (`GameScreen`: HUD, overlays, routes, timing, input, replay, debug, ~2 KB) and `assets` (`GameAssets`: shared sprites and sounds, loaded once per `AppSession` and lent to every game), plus `cache`, a borrowed pointer to the session's `AssetCache` (`src/shared/asset_cache.h`): a reference-counted cache, keyed by path, of the files a level names and of the player sheet, HUD icon and font. Replay, Retry and re-entering a level read nothing from disk (was 15 textures, the music and the font on `01_lugio_01`); another level reads only the files it adds, and what it does not use is unloaded once it has opened, so the cache holds one level's files (at most 15 textures, 3.1 MiB). The start menu's font, logo and confirm sound are in the same cache and the session holds them between menus, so reopening the menu after Level Select reads nothing (was 1 texture, 1 sound and a font handle each time). Headers that only pass `GameState *` include `src/game_fwd.h`; `levels/level.h` does not include `game.h`, so touching `game.h` recompiles 65 of 231 headless objects (was 158).

## Time-trial ghost

On `main` since 2026-10-09.

Each normal run records one position/sprite sample per fixed step; a finished run that beats the stored one becomes the level's ghost (TOML next to the profile, or a per-level `localStorage` entry), bound to the level's content hash and capped at five minutes. Later attempts race a translucent ghost; Settings has **Ghost (best run)** on/off (`src/core/game_ghost.c`, `game_ghost_file.c`).

## Mid-level Continue

On `main` since 2026-10-09.

The profile (format_version 2) keeps one `[resume]` Continue point: respawn point, score, lives, collected coins and timer, bound to the level's content hash. Recorded on a new respawn point, a pause or leaving part-way; cleared on finishing or losing the level; offered by the menu's **Continue** and `--continue` (`src/core/game_resume.c`).

## Audit remediation

Merged 2026-10-07, PRs #306, #308, #309.

Both audit rounds merged in #306; code-scanning cleanup in #308/#309 (0 open alerts); manual moved to an Astro content collection and re-audited against the code.

## Fixed-step simulation

On `main` since 2026-10-06.

`src/core/game_timing.c` feeds frame time into an accumulator and advances fixed 1/60 s steps; `make timing-lab` compares variable and fixed steps.

## Pages publishing

On `main` since 2026-10-06.

`pages-build` → `pages-deploy` jobs at the end of `build.yml` use the same run's WASM artifact; only the deploy job holds Pages/OIDC permissions.

## CI gates

`-Werror` on every native leg and the Web leg, sanitizers with fuzz-seed replay, scripted replay smoke, Windows-only real-backend desktop job (Mesa llvmpipe, `RAYLIB_AUDIO=null`), Windows Clang job, WebAssembly artifact checks, always-on docs drift job plus docs lint/build, CodeQL (C/C++ SARIF filtered for third-party code-quality noise; Actions, Python, JS/TS), release `SHA256SUMS` and build provenance attestations.

## raylib 6.2 port

Game and editor use pinned, checksum-verified raylib 6.2 source (`vendor/raylib/manifest.json`, `tools/build_raylib.py`) on desktop (bundled GLFW) and Web (the Makefile's `EMSCRIPTEN_VERSION`). SDL2 and its satellite libraries are gone.

## Runtime level loader

Bare game launch loads the v1 campaign manifest and opens its selector; `--level <path>` / `make run-level LEVEL=...` loads TOML directly.

## Authored checkpoints

Optional `[[checkpoints]]` records provide runtime respawns, editor placement/editing/feedback, serializer support, and test coverage.

## Level editor

Standalone raylib editor builds with `make editor` / runs with `make run-editor`; reads/writes TOML-backed `LevelDef` data and playtests private TOML snapshots.

## Level validation

`tools/validate_levels.py`, `make validate-levels` (CI on Linux, macOS and Windows), and C validation tests.
