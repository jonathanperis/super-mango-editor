# Design Decisions

<a id="home"></a>

---

The larger choices behind the code, one entry each: what the situation was,
what I chose, and what that means for you when you read or change the code.
Every fact here comes from the repository: a commit, a spec, a comment or
another manual page, named at the end of each entry. When the code changes
one of these choices, the entry gets a new paragraph instead of being
rewritten, so the reasoning stays traceable.

---

## raylib instead of SDL2

**Context.** Until 2026-09 the game and editor used SDL2 with SDL2_image,
SDL2_ttf and SDL2_mixer. The migration plan asked to take SDL off the project
entirely and use one pinned source version for every target instead of
whatever each OS packaged.

**Decision.** raylib, built from pinned, checksum-verified source by
`tools/build_raylib.py`: GLFW on the desktop, Emscripten's GLFW in the
browser, and no SDL backend or SDL-named shims. The port landed on
2026-09-17 (`09b64a1`); the pinned release has been updated since, on its own
(`eb36992` was the first such update).

**Consequences.** One dependency covers window, input, graphics, text and
audio, and `vendor/raylib/manifest.json` is the only place its version lives.
Text uses raylib's built-in font, so no font file ships. Profile bindings for
inputs raylib cannot see (media keys, controller paddles) are kept and shown
as unavailable instead of being reset. Collision stays on integer rectangles
and converts to raylib's float `Rectangle` only when drawing.

Sources: `.specs/archive/SPEC.md` (C17–C23, I26–I27), `vendor/raylib/README.md`.

## TOML for levels

**Context.** Levels were JSON read with cJSON. Designers edit level files by
hand, and a level is mostly lists of placements.

**Decision.** TOML, parsed by the vendored `tomlc17` (2026-04-04, `1aa6c42`).
TOML allows comments, its `[[array]]` tables map one-to-one onto placement
lists, and it has no brace noise. TOML is the only runtime level format; the
in-memory form is `LevelDef` (D-001 in the
[level editor spec](https://github.com/jonathanperis/super-mango-editor/blob/main/.specs/features/level-editor/spec.md)).

**Consequences.** One schema table, `ROOT_FIELDS` in
`src/shared/serializer_schema.c`, names every key, its type, the struct member
that holds it and its default; the loader and the writer both walk it, and
`tests/schema_parity_test.py` keeps the Python validator's copy in step.
`tomlc17` carries project patches, recorded in `vendor/tomlc17/README.md`.

## A fixed simulation step

**Context.** Each frame used to simulate its own measured `dt`. Integration
error, jump apexes and per-frame movement changed with the display's refresh
rate, and replays only matched live play at 60 Hz.

**Decision.** Real frame time feeds an accumulator that runs fixed
1/60 s steps (`GAME_FIXED_STEP`), at most `GAME_MAX_STEPS_PER_FRAME` (five) per frame
(2026-10-06, `5df4ed1`; D-004).

**Consequences.** A jump reaches the same height at 30, 60 or 144 Hz, and
smoke runs, scripted replays, experiment captures and ghosts are reproducible.
Drawing is no longer tied to the step: sprites are blended between the last
two steps on fast displays (`src/core/game_movers.c`). `make timing-lab`
shows the problem this solved. Replays across platforms also need the same
float results, hence `-ffp-contract=off` and `game_sinf`/`game_cosf`.

Sources: `src/core/game_timing.h`, [Architecture](../architecture/#fixed-time-step).

## Tables of entity rows

**Context.** One fixed step called helpers spread over four files, so a reader
had to open all of them to learn what a step does and in what order.

**Decision.** What happens to each entity kind, and when, is a table of
`{ name, function }` rows walked top to bottom by one loop: `s_level_loaders`,
`s_update_steps`, `s_damage_sources`, `s_movers` and `s_render_layers`
(first table on 2026-10-10, `96ea8ca`). Each row calls a small adapter, and the
entity modules stay unchanged.

**Consequences.** The order is readable in one place, and `make docs-drift`
compares the update and render tables with the tables in
[Architecture](../architecture/#update-order-one-fixed-step). Adding a kind
means its module plus one row (and adapter) in each table it takes part in;
the [Entity Walkthrough](../entity-walkthrough/) has the checklist.

## The editor's own UI, not raygui widgets

**Context.** The editor needs text fields with selection, Tab focus, undo
hooks and tests that run without a window.

**Decision.** A small immediate-mode UI of my own (`src/shared/ui.c` and
friends), drawn with raylib and its built-in font (D-002). From raygui I take
only its icon bitmaps, generated into `vendor/raygui/raygui_icons.h`.

**Consequences.** The widgets have what raygui does not offer (text
selection, Tab focus, undo hooks, headless tests), and `editor_ui_test`
checks them, icons included, pixel by pixel. The cost is code I maintain
myself. `make raygui-icons` is the only time raygui is downloaded.

Sources: `vendor/raygui/README.md`.

## QOA for the generated sounds

**Context.** The twelve generated sounds were 16-bit WAVs: 1,041,286 bytes,
nearly all of the browser build's preloaded data.

**Decision.** Ship them as QOA ("Quite OK Audio"), which raylib reads natively
(2026-10-16, `9301870`). `tools/qoa.py` writes the same bytes as raylib's own
encoder, so `make docs-drift` can check the sounds without a C build.

**Consequences.** The sounds dropped to 210,832 bytes and the browser's
`.data` file by 72 %. QOA is lossy, so `tools/gen_sounds.py --check` accepts
a file that is byte-identical or decodes within 2 % RMS of a fresh encoding.
WAV still loads, so a level may name a `.wav` of its own.

Sources: [Sounds](../sounds/), [Assets](../assets/#generated-sounds).

## `ScriptProcessorNode` for browser audio

**Context.** In the browser, raylib's miniaudio plays through a
`ScriptProcessorNode`, which Chromium logs as deprecated. Its
`AudioWorkletNode` path needs Wasm Workers and `SharedArrayBuffer`, and that
needs a cross-origin isolated page (COOP/COEP headers). GitHub Pages cannot
send custom headers.

**Decision.** Keep `ScriptProcessorNode` for now (Round 5, PR #477). The
other options, a service worker that adds the headers or a host that sends
them, are written down and wait for the owner's decision.

**Consequences.** Audio works in every browser today, and the live site check
lists the deprecation warning as known noise. Moving to AudioWorklet means
changing that list too.

Sources: [Build System, "Web audio"](../build-system/#make-web).

## A shared, reference-counted asset cache

**Context.** A Replay closes one `GameState` and opens another on the same
level, and each game loaded and freed its own copy of the level's images and
music.

**Decision.** `AssetCache` (`src/shared/asset_cache.h`, 2026-10-10,
`63d4803`): one entry per file path, counting its holders. An entry nobody
holds is not unloaded at once; it waits for `asset_cache_trim`, which runs
after the next level has loaded.

**Consequences.** Replay, Retry and re-entering a level read nothing from
disk, and the cache holds about one level's files. `GameState.cache` is a
pointer borrowed from `AppSession`; a game without a session (the tests) gets
`NULL` and loads private copies.

Sources: [Architecture, "The asset cache"](../architecture/#the-asset-cache-files-that-outlive-a-game).

## Size-optimised web build, no LTO

**Context.** The web page is the one place players wait for a download, and
the game asks little of the CPU there: WebGL draws the frames.

**Decision.** The web game and its raylib compile with `-Os` instead of `-O2`
/ `-O3`, and the link targets the browser only (`ENVIRONMENT=web`;
2026-10-21, `4191b4c`). No build uses link-time optimisation: it would make
release builds differ more from the debug builds the tests run (Makefile
comment above `PATH_MAP_CFLAGS`).

**Consequences.** Measured at the time: `super-mango.wasm` went from 469 KB to
371 KB (190 KB to 166 KB gzipped) and the work in each animation frame stayed
at a 0.4 ms median. Native release builds stay at `-O2`, with unused sections
dropped by the linker instead.

## A headless Memory backend for tests

**Context.** CI runners and SSH sessions often have no display or sound card:
hosted macOS runners offer no OpenGL pixel format, and the game will not
start without an audio device.

**Decision.** `RAYLIB_PLATFORM=memory` builds raylib with its software
framebuffer and miniaudio's null backend, in a separate `OUTDIR`; the gating
macOS and Windows legs run the tests that way. `RAYLIB_AUDIO=null` keeps the
real desktop backend with a null playback device.

**Consequences.** `make test` runs anywhere. These builds do not certify real
windows, resizing, dialogs or GPUs, so `release` and `dist-native` refuse
both variants, Linux CI keeps a real GLFW/OpenGL run under Xvfb, and a real
macOS window is checked by hand ([Release Checklist](../release-checklist/)).

Sources: `.specs/archive/SPEC.md` (C23), [Build System](../build-system/#builder-and-build-modes).

## The site on its own domain

**Context.** The website and browser game first lived at
`jonathanperis.github.io/super-mango-editor/`, sharing browser storage with
every other GitHub Pages site on the account (D-005, 2026-10-10).

**Decision.** On 2026-10-21 the site moved to
its own custom subdomain (`d9982fe`, `docs/public/CNAME`).
On 2026-11-27 the owner retired that domain and the site
moved to `https://jonathanperis.dev/super-mango-editor/`, a path on the
account's GitHub Pages custom domain (no `CNAME`; Astro `base` is
`/super-mango-editor`).

**Consequences.** Profiles and ghosts in browser storage again share an origin
(`https://jonathanperis.dev`) with the account's other Pages sites, so the game
keeps treating stored values as untrusted. Saves made at older addresses stay
there. `SECURITY.md` describes the current scope.
