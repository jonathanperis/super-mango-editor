# Super Mango — Project Vision

## Overview

A 2D pixel art platformer written in C11 + SDL2, designed as a learning resource for game development. Every line of code is documented for someone who knows basic programming but is new to C and SDL2.

## Goals

- Fun, polished 2D platformer experience
- Serve as a teaching codebase for C/SDL2 game development
- Multi-level design with increasing difficulty
- Cross-platform: macOS (primary), Linux, Windows, WebAssembly

## Current State

- Three TOML levels in `levels/`, runtime TOML loader via vendored `tomlc17`, and `next_phase` transitions
- Full player mechanics, 6 enemy types, 7 hazard types, collectibles, climbable surfaces, and level-completion summary
- Dynamic multi-screen worlds, 32 render layers, delta-time physics at 60 FPS
- Start menu, HUD, lives system, debug overlay, keyboard/gamepad hot-plug support
- Builds natively on macOS, Linux, Windows, plus WebAssembly via Emscripten
- Standalone SDL2 level editor is shipped with TOML save/load, exporter, validation blocking, playtest, recent files, autosave, and smoke-test mode
- CI runs native builds, editor builds, 11-test suite, level validation, native smoke, WebAssembly artifact smoke, docs lint/build, and CodeQL

## Next Milestone

- **Editor Quality + Campaign Flow**: richer validation diagnostics, metadata editing, autosave recovery, and multi-level campaign tooling on top of the shipped editor
