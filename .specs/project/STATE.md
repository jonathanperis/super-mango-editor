# Project State

## Active Work

| Item | Status | Notes |
|------|--------|-------|
| Level Editor | Ready to implement | 3rd revision complete — game-accurate rendering details, sandbox_00 validation checklist |

## Decisions (Finalized)

### D-001: JSON + C Pipeline
JSON is canonical. C exporter generates `level_XX.c` + `.h`. Game never links cJSON.

### D-002: Custom SDL2 + SDL2_ttf UI
~7 immediate-mode widgets. No external UI library.

### D-003: No Game JSON Loader
Game reads only compiled `const LevelDef`. Play-test: Ctrl+E export → `make run`.

## Critical Implementation Details

### Entity Display Sizes ≠ Sprite Frame Sizes
- Spider/Jumping Spider: frame 64×48, display 64×10 (art band at y=22)
- Bird/Faster Bird: frame 48×48, display 48×14 (art band at y=17)
- Bouncepad: frame 48×48, display 48×18 (crop at y=14)
- Vine: tile 16×48 → display 16×32 (crop y=8), step=19px
- Ladder: tile 16×48 → display 16×22 (crop y=13), step=8px
- Rope: tile 16×48 → display 12×36 (crop x=2, y=6), step=34px
- Spike Block: frame 16×16, display 24×24
- Coin: display 16×16 (scaled from texture)
- Last Star: display 24×24

### Derived Y Positions (NOT in LevelDef)
- Spider/J.Spider: y = FLOOR_Y - 10 = 242
- Fish/Faster Fish: y = (300 - 31) - 24 = 245
- Axe Trap: y = FLOOR_Y - 3*48 + 16 = 124
- Circular Saw: y = FLOOR_Y - 2*48 + 16 - 32 = 140
- Spike Row: y = FLOOR_Y - 16 = 236
- Bouncepad: y = FLOOR_Y - 18 = 234
- Platform: y = FLOOR_Y - tile_height*48 + 16

### Floor 9-Slice Algorithm
- grass_tileset.png: 48×48, 3×3 grid of 16×16 pieces
- Edge caps at sea gap boundaries and world edges
- Must replicate for accurate Sandbox preview

### Auto-Derived Entities
- Blue flames: derived from sea_gaps, editor shows ghost preview
- Rail-riding entities (spike_blocks, RAIL float_platforms): position from rail interpolation

## MAX_* Constants Reference

| Constant | Value |
|----------|-------|
| MAX_SEA_GAPS | 8 |
| MAX_RAILS | 4 |
| MAX_PLATFORMS | 8 |
| MAX_COINS | 24 |
| MAX_STAR_YELLOWS | 3 |
| MAX_SPIDERS | 4 |
| MAX_JUMPING_SPIDERS | 4 |
| MAX_BIRDS | 4 |
| MAX_FASTER_BIRDS | 4 |
| MAX_FISH | 4 |
| MAX_FASTER_FISH | 4 |
| MAX_AXE_TRAPS | 4 |
| MAX_CIRCULAR_SAWS | 4 |
| MAX_SPIKE_ROWS | 4 |
| MAX_SPIKE_PLATFORMS | 4 |
| MAX_SPIKE_BLOCKS | 4 |
| MAX_BLUE_FLAMES | 16 |
| MAX_FLOAT_PLATFORMS | 6 |
| MAX_BRIDGES | 2 |
| MAX_BOUNCEPADS_SMALL | 4 |
| MAX_BOUNCEPADS_MEDIUM | 4 |
| MAX_BOUNCEPADS_HIGH | 4 |
| MAX_VINES | 24 |
| MAX_LADDERS | 8 |
| MAX_ROPES | 8 |

## Deferred Ideas

- Tilemap painting for custom floor/gap layouts
- Multi-level campaign editor with level ordering
- Undo history saved to disk for crash recovery
- Hot-reload: game reads JSON directly (v2)
- Play-test shortcut F5 (v2)
- Animated entity previews (v2)
