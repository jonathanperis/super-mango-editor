# Super Mango — Project Vision

## Overview

A 2D pixel art platformer written in C11 + raylib 6.0, designed as a learning resource for game development. Every line of code is documented for someone who knows basic programming but is new to C and raylib.

## Goals

- Fun, polished 2D platformer experience
- Serve as a teaching codebase for C/raylib game development
- Multi-level design with increasing difficulty
- Cross-platform: macOS (primary), Linux, Windows, WebAssembly

## Current State

- Three playable TOML levels in `levels/` plus six learning labs in `levels/labs/`; the v1 `levels/campaigns/main.toml` menu catalog runs Creator's Playground → Volcanic Depths 1 → Volcanic Depths 2, with runtime TOML loading via vendored `tomlc17` and `next_phase` transitions
- Full player mechanics, authored checkpoint respawns with legacy fallback, 6 enemy types, 7 hazard types, collectibles, climbable surfaces, and level-completion summary
- Dynamic multi-screen worlds, 32 render layers, fixed 1/60 s physics steps driven by a frame-time accumulator
- Campaign-driven start menu, HUD, lives system, debug overlay, keyboard/gamepad hot-plug support
- Builds natively on macOS, Linux, Windows, plus WebAssembly via Emscripten
- Standalone raylib level editor is shipped with TOML save/load, checkpoint palette/canvas/properties feedback, validation blocking, private-snapshot playtest, recent files, recovery snapshots, and smoke-test mode
- CI runs native builds, editor builds, 15-native-binary test suite plus Python host checks, level and campaign-manifest validation, native smoke, WebAssembly artifact smoke, docs lint/build, and CodeQL

## Next Milestone

- **Editor Quality + Campaign Flow**: richer validation diagnostics, metadata editing, autosave recovery, and manifest-editing support on top of the shipped selector
