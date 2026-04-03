---
name: Asset & sound conventions
description: Sprite sheet dimensions, file naming, audio format rules for this project
type: project
---

## Sprite sheets

- All frames are **48×48 px** (the "Super Mango 2D Pixel Art Platformer Asset Pack" by Juho)
- PNG files in `assets/`, snake_case, named after the component (`spider.png`, `float_platform.png`)
- Unused assets in `assets/unused/` (do not delete; reserved for future use)
- Player sheet: 192×288 → 4 cols × 6 rows × 48×48 (rows: idle, walk, jump, fall, climb, ?)

## Animation row conventions

| Row | Animation | Loop? |
|-----|-----------|-------|
| 0 | Idle | Yes |
| 1 | Walk/Run | Yes |
| 2 | Jump (rise) | No |
| 3 | Fall | No |
| 4 | Climb | Yes |
| 5 | (unused/death) | — |

## Audio

- Music: `sounds/game_music.ogg` — streamed via `Mix_LoadMUS`, looped, volume 10%
- SFX: all `.wav` in `sounds/`, named after the component
- Unused: `sounds/unused/`, `sounds/game_music.wav` (16MB WAV duplicate, never load this)
- Non-fatal load pattern always applies to sound

## Analysis script

```sh
python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png
```
