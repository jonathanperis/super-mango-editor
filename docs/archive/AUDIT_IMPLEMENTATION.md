# Sandbox School — Audit Implementation Report

> **Historical report, dated 2026-09-16:** it describes the repository at that time, so use the [Builder Manual](https://super-mango-editor.jonathanperis.tech/docs/) for current behaviour.

Date: 2026-09-16. Local branch: `feature/sandbox-school-roadmap`.
Baseline: `042003d`, with the pre-existing instrumentation cleanup preserved.

## Delivered

| Audit area | Implementation | Evidence |
|------------|----------------|----------|
| Broken default startup | Restored the ordered three-stage campaign manifest; removed stale onboarding dependencies | Full native suite; campaign/menu/session cases |
| Teaching accuracy | Corrected ownership, union, memory-size, timing and source-boundary explanations | Source review; docs drift; Astro check/build |
| Guided learning | Eight labs, six standalone mechanics levels and complete collectible integration walkthrough | `docs/wiki/learning-path.md`, `mechanics-museum.md`, `entity-walkthrough.md`; nine-level validation/smoke |
| Running experiments | Freeze, single-step, slow motion, bounded live movement tuning/reset, player/contact display, fish/platform/saw state inspection | `game_inspector.c`; simulation contract scenarios |
| Reproduction | Explicit capture/export/import, per-step dt/input/tuning, seed and active-level byte fingerprint; portable unsigned PRNG | 180-step round-trip with opposing live input; malformed import and stale-source rejection |
| Missing sprites | Required gameplay sprites checked before committing an initial/next level | Missing-saw failure scenario; lifecycle tests |
| Collision timing | Moving hazards update before collision sampling | Saw enters the player during the same tested step |
| Editor workflow | Builder/run-editor build both executables; missing sibling binary gives instructions; playtests use `--no-save` | Native builds and editor tests; launch code review |
| Storage and boundaries | Shared serializer, file I/O and widgets moved to `src/shared/`; sparse config snapshot ownership in undo | Native/sanitizer/editor tests; overflow/branch/clear history proof |
| Distribution | Isolated debug/release modes; optimized native builder archives include both executables; WASM packaging reuses existing output | Debug/release builds; native and WASM archive checks |
| Asset cost | Excluded reserve assets from native bundles and WASM preload; generated inventory and 40 MiB raw budget | Inventory, packaging tests, successful Emscripten builds |
| Public facts | Website consumes generated campaign/lab/asset data; root public docs participate in CI path filters | Generated freshness; docs drift; 24-page Astro build |
| Third-party notices | Parser MIT notice and provenance documentation packaged; available MSYS2 package notices copied on Windows | Archive tests; source-page review |
| Test visibility | Session cases initialize independently and report individual results; stable runtime fixtures isolate geometry from showcase edits | 19 reported session cases, including the grouped simulation scenarios |

## Measurements

- Playable assets: **35,802,693 raw bytes**. Excluded reserve files: **13,768,354 bytes**.
- These are file sizes, not compressed network-transfer measurements.
- Base undo storage on arm64: **352,272 bytes**, previously **6,606,856 bytes**.
  Configuration edits additionally allocate their own before/after snapshots;
  total history remains bounded. This is a storage reduction, not an FPS claim.
- Current campaign facts: **3 levels, 23 screens, 38 enemies, 56 hazards,
  71 collectibles**, plus **6 separate learning levels**. Generated JSON is
  authoritative for subsequent edits.

## Verification

Passed locally on macOS/Apple Silicon:

- All **15 native test binaries**, Python level checks and JS host/storage/touch/archive contracts.
- All 15 binaries under **AddressSanitizer/UBSan**, with affected session checks rerun after final code edits.
- **Nine 120-frame sanitized level smoke runs**, editor smoke, and a debug-inspector smoke run.
- **27 optimized-build replay scenarios**, each executed twice with state assertions.
- Default/debug/release game and editor builds.
- Both normal/debug WebAssembly builds with `EM_FROZEN_CACHE=1`, without dependency installation.
- JavaScript syntax, `WebAssembly.compile` and normal/debug archive checks.
- Native builder archive generation with both binaries and notices.
- Six selected changed C modules checked by Clang static analysis; no diagnostics.
- Level validation, generated inventory/catalog/overlay freshness, docs drift and roadmap checks.
- Astro check: **zero errors, warnings or hints**; Astro build: **24 pages**.
- `git diff --check`.

Local archives are in `out/school-dist/`. The installed Emscripten version is
5.0.7-git; CI pins 4.0.23. Browser/GPU/gamepad/native-dialog interaction,
Linux/Windows execution and a fresh remote CI run were not performed. The browser
export path is compiled and source-reviewed, but an actual download interaction
is not claimed verified. Node prints its pre-existing `module.register()`
deprecation notice during docs tooling.

## Final review corrections

1. Lowered/slowed the moving-platform lab so a normal jump can reach it.
2. Tied capture fingerprints to the bytes used to load the active level; external
   edits cannot silently label an old simulation with a new file's fingerprint.
3. Reserved inspector keys during debug remapping and made replay's recorded
   timing take precedence over live speed changes.
4. Fixed the climbing example and closed Python/runtime climbable-height drift,
   including C float narrowing at valid world edges. The initial Python-only
   pass had missed a rope extending two pixels below the world; runtime smoke
   caught it, and the affected checks were rerun after correction.

Regression scan: 38 callers checked, 44 assertions checked, 4 flagged/fixed.

The counted references are selected inspection/capture/resource/history call
sites and validator callers, plus simulation/archive/bounds assertions. Shared
header consumers, Makefile recipes, generated facts and documentation were also
reviewed; these counts are not coverage percentages.

## Specification review

The active follow-up is scoped by C6–C8; earlier spec phases remain historical.

- **Constraints — HOLD:** canonical branch/workspace, preserved cleanup and
  showcase data, C11/SDL2 architecture, no publication/installation/browser
  interaction, explicit unresolved media provenance.
- **Interfaces I14–I20 — MATCH:** campaign/fixtures, learning content, inspector,
  required textures/ordering, packaging, shared ownership/undo, generated facts.
- **Acceptance A19–A26 — PASS with stated platform evidence limits:** startup and
  checks restored; learning/inspection/runtime/builder/ownership/facts delivered;
  verification and final review completed. Proof is mapped in the table above.
- **Invariants V10–V11 — HOLD:** blockers retain simulation ownership
  (`game_inspector_step`, simulation tests); packaging preserves verified WASM
  outputs and includes vendored notices (`package_release.py`, archive tests).
- **Tasks T21–T29 — VERIFIED:** local implementation and evidence complete.
- **Design gate — GO:** bounded explicit state and existing test/persistence
  patterns satisfy this scope. No remaining local code blocker was found in the
  reviewed contracts.

## Remaining external evidence

JuhoSprite's pack page lists CC BY 4.0 metadata and also says free use/no credit
needed; attribution, the license link and modification notes are retained.
The pack explicitly disclaims authorship of Round 9x13. Original font/audio
license records still require the acquisition sources/rightsholders; they have
not been invented or relabeled as MIT. See `THIRD_PARTY_NOTICES.md` and the
Asset Provenance manual page.

Full interactive playthrough and physical-device/browser compatibility remain
testing follow-ups. No commit, push, release publication or deployment was made.

### Status, 2026-10-21

The paragraphs above are left as written on 2026-09-16.

- **Audio — resolved for current `main`.** Commit `3157f77` (2026-10-06)
  replaced every shipped WAV with sounds synthesized by `tools/gen_sounds.py`
  (no recorded or sampled material) and deleted the unused, unlicensed
  reserves; `7af2722` (2026-10-16) moved them to QOA, encoded by `tools/qoa.py`.
  They are project work under `LICENSE`, and `make docs-drift` fails if a
  committed sound is not the generator's output. The older music and ambience
  files (`game_music.ogg`, the `sounds/*.mp3` and `*.ogg` files) had already been
  removed by April 2026.
- **Font — resolved for current `main`.** Commit `1c6fb8d` (2026-10-06)
  removed `round9x13.ttf`; all text now uses raylib's built-in default font,
  covered by raylib's zlib/libpng licence. No font file ships.
- **Sprites — unchanged.** JuhoSprite credit and the CC BY 4.0 link stay in
  `THIRD_PARTY_NOTICES.md` and the Asset Provenance page.
- **Still open for the owner.** Where `round9x13.ttf` and the replaced
  WAV/OGG/MP3 files originally came from, and under what licence, is not
  recorded anywhere in the repository or its history. They remain in Git
  history, and the releases published before 2026-10-06 were built while they
  were in the tree (checked: the v1.0.845 Linux archive carries
  `assets/fonts/round9x13.ttf` and the original WAVs). The v1.0.976 archive and
  later ones ship the generated audio and no font file. Whether to keep the
  older releases published is the owner's call.

## Try it

```sh
make builder
make run-level-debug LEVEL=levels/labs/01_collision.toml
make run-editor
make timing-lab
```

In debug mode: F2 freeze, F3 step, F4 speed, F6 property, minus/equal tune,
F7 reset, F8 restart/record, F9 export, F10 inspect an entity.
