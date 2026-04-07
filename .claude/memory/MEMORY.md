# Memory Index — Super Mango Editor

Project-level memory for Claude Code sessions working on this repository.
Each entry is a one-line hook; the file holds the full context.

- [Architecture & GameState contract](architecture.md) — single-file GameState by value; no heap; all entities in fixed arrays
- [Coding standards & comment style](coding_standards.md) — learning-resource intent; every SDL call explained; WHEN/THEN error handling
- [Asset & sound conventions](assets.md) — 48×48 frames; snake_case filenames; all audio .wav except music .ogg
- [Known gaps & deferred work](gaps.md) — overlay screens, start menu play-through, death animation, SFX volume
- [Per-frame texture allocation issue](perf_per_frame_tex.md) — start_menu and gamepad HUD message create/destroy SDL texture every frame
- [Branch conventions](branches.md) — feature/overall-improvements is the active improvement branch
