---
name: Known gaps & deferred work
description: Features confirmed missing or deferred after the 2026-04-02 investigation pass
type: project
---

## P1 — Missing feedback loops (agreed to implement later)

- **No level-complete screen**: `last_star.collected = 1` is set but nothing happens — no transition
- **No game-over screen**: `lives < 0` resets silently with no player feedback
- **No in-game pause**: ESC quits immediately; no pause overlay exists
- **Start menu is dev-only**: Shows "run with: make run-sandbox" instead of a playable entry point

**Why deferred:** 2026-04-02 session pivoted to code structure/optimization first.
**How to apply:** When implementing these, use `OverlayState` enum approach (discussed in session). Do NOT create new modules — add to `game.c` and `game.h` only.

## P3 — Polish (lower priority)

- Player falls into sea gap with no death animation — just disappears
- Music hardcoded to 10% volume; no runtime volume control
- No high-score persistence

## Investigation notes

- `src/game.c` is large (~600+ lines for game_loop_frame alone); candidate for splitting render/update helpers
- `src/game.c:1476-1495` — gamepad HUD message creates a new SDL texture every frame (per-frame allocation)
- `src/screens/start_menu.c:95-156` — three text strings each create and destroy an SDL surface+texture every frame
