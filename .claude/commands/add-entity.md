---
description: "Add a new game entity (enemy, collectible, hazard, platform, decoration) with full lifecycle: header, source, GameState wiring, and debug hitbox."
argument-hint: "Entity name and description (e.g. 'spike_ball — rotating spike ball hazard')"
---

# Add New Entity

Create a complete new entity following the project's established patterns.

## Steps

1. **Parse the request**: extract the entity name (snake_case) and its purpose from $ARGUMENTS.

2. **Analyze the sprite sheet**: if an asset exists for this entity, run:
   ```sh
   python3 .claude/scripts/analyze_sprite.py assets/<entity_name>.png
   ```
   Use the output to determine FRAME_W, FRAME_H, frame count, and animation rows.

3. **Create the header** (`src/<entity_name>.h`): define the struct and declare lifecycle functions following the entity template.

4. **Create the source** (`src/<entity_name>.c`): implement init, update, render, cleanup with full inline comments explaining SDL2 calls, physics, and numeric values.

5. **Wire into GameState** (`src/game.h`):
   - Add `#include "<entity_name>.h"`
   - Add `SDL_Texture *<entity_name>_tex` for the shared texture
   - Add the entity array and count fields
   - Add `Mix_Chunk *snd_<entity_name>` if it has a sound

6. **Wire into game.c**:
   - Load texture in `game_init`: `IMG_LoadTexture(gs->renderer, "assets/<entity_name>.png")`
   - Load sound if applicable: `Mix_LoadWAV("sounds/<entity_name>.wav")`
   - Call update in the game loop's update section
   - Call render in the correct layer order (back-to-front)
   - Free resources in `game_cleanup` (reverse init order, set NULL)

7. **Add to sandbox.c**: place instances in `sandbox_load_level()` and `sandbox_reset_level()`.

8. **Add debug hitbox** in `debug.c`: draw the collision rect with a descriptive color and comment.

9. **Build and verify**: `make clean && make`

## Entity Template Reference

@.claude/references/entity-template.md

## Coding Standards

@.claude/references/coding-standards.md
