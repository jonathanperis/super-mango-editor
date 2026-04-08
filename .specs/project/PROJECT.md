# Super Mango — Project Vision

## Overview

A 2D pixel art platformer written in C11 + SDL2, designed as a learning resource for game development. Every line of code is documented for someone who knows basic programming but is new to C and SDL2.

## Goals

- Fun, polished 2D platformer experience
- Serve as a teaching codebase for C/SDL2 game development
- Multi-level design with increasing difficulty
- Cross-platform: macOS (primary), Linux, Windows, WebAssembly

## Current State

- Single-level game with full player mechanics, 6 enemy types, 7 hazard types, collectibles, and climbable surfaces
- 1600px scrolling world (4 screens), 32 render layers, delta-time physics at 60 FPS
- Start menu, HUD, lives system, debug overlay
- Keyboard and gamepad support with hot-plug
- Builds natively on macOS + WebAssembly via Emscripten

## Next Milestone

- **Level Editor**: standalone visual editor for creating and editing levels, removing the need for manual C struct definitions
