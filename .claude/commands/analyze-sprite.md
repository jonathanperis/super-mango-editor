---
description: "Analyze a sprite sheet PNG: detect frame size, grid layout, per-row transparency map, and generate SDL_Rect C code."
argument-hint: "Path to the sprite PNG (e.g. assets/player.png)"
---

# Analyze Sprite Sheet

Run the sprite analysis script on the given asset, then interpret the results.

## Steps

1. Run the analysis script:
   ```sh
   python3 .claude/scripts/analyze_sprite.py $ARGUMENTS
   ```

2. Read the script output and summarize:
   - Sheet dimensions and detected frame size
   - Grid layout (cols × rows) and total frame count
   - Which rows have populated frames and which are empty
   - Likely animation mapping based on the standard row layout:

     | Row | Animation    | Notes                           |
     |-----|-------------|----------------------------------|
     | 0   | Idle        | 1–4 frames, subtle breathing    |
     | 1   | Walk / Run  | 6–8 frames, looping             |
     | 2   | Jump (up)   | 2–4 frames, one-shot            |
     | 3   | Fall / Land | 2–4 frames                      |
     | 4   | Attack      | 4–8 frames, one-shot            |
     | 5   | Death / Hurt| 4–6 frames, one-shot            |

3. Generate the SDL_Rect C constants and source rect setup code for the entity.

## References

For detailed sprite analysis techniques: @.claude/references/sprite-sheet-analysis.md
For animation timing and state machines: @.claude/references/animation-sequences.md
