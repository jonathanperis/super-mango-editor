# Goobma's Lessons Learned — Pixel Art Rules

Hard-won rules from real sprite work. Every Goobma instance must follow them.

---

## Lesson 1: ALWAYS analyze existing assets before creating

Never generate a sprite blindly. Run `analyze_sprite.py` on every existing asset in the target category first. Study the envelope, palette, padding, and frame layout. Your new sprite must belong to the same visual family.

**Rule:** Before touching any canvas, run:
```sh
python3 .claude/scripts/analyze_sprite.py assets/sprites/<category>/<existing>.png
```
Match the envelope, palette, and padding exactly.

---

## Lesson 2: Match dimensions — no exceptions

If all backgrounds are 400×300, yours must be 400×300. If all entity frames are 48×48, yours must be 48×48. Dimension mismatches break the renderer and look wrong at 2× scaling.

**Rule:** Verify dimensions against category peers before generating. Never guess.

---

## Lesson 3: Silhouette test first

If the sprite isn't readable as a solid black shape, no amount of detail will save it. Design the shape before the pixels.

**Rule:** Fill your sprite solid black mentally. Can you still tell what it is? If not, simplify the shape.

---

## Lesson 4: Hue-shift shadows, don't just darken

A green leaf's shadow is blue-green, not dark green. Shift the hue toward blue/purple for shadows, toward yellow/orange for highlights. Pure darkening flattens the art.

**Rule:** When shading, change hue AND value, not just value.

---

## Lesson 5: Limited palette per sprite

4-8 colors for small sprites (16×16), 8-16 for larger ones. Fewer colors = more cohesive look. Avoid pure black outlines — use dark versions of the sprite's dominant color instead.

**Rule:** Count your colors. If you're over the limit, merge similar shades.

---

## Lesson 6: Never change the silhouette for theme variants

When creating themed variants (fire, ice, desert), only change colors. The hand-pixeled detail and shape are sacred. Use `remap_colors()` — load the PNG, swap colors, save.

**Rule:** Theme variants are palette remaps only. Same pixel positions, different colors.

---

## Lesson 7: Avoid single-pixel noise

Isolated pixels that don't connect to a shape read as dirt/artifacts at 2× scaling. Every pixel must connect to a neighbor of the same color or be intentionally placed for anti-aliasing.

**Rule:** Zoom to 2×. If a pixel looks like a mistake, it probably is.

---

## Lesson 8: Idle is never still

A breathing cycle (1-2px vertical shift every 0.5s) makes characters feel alive. Even at pixel scale, squash and stretch sell the motion.

**Rule:** Animation frames should show life, not just pose changes. Add subtle breathing to idle states.

---

## Lesson 9: 9-slice tilesets must tile seamlessly

Tilesets are 48×48 divided into a 3×3 grid of 16×16 pieces. Each piece must work in any position (corner, edge, center). The rightmost column must seamlessly connect to the leftmost column when tiled.

**Rule:** Test tilesets by tiling them 3×3 before shipping. Check all edge combinations.

---

## Lesson 10: Save to the correct category folder

Sprites live under `assets/sprites/<category>/<name>.png`. Wrong folder = broken asset path = missing texture in-game.

**Rule:** Verify the category folder exists before saving. Use the correct path:
- `backgrounds/` — parallax layers
- `foregrounds/` — fog, water, lava strips
- `collectibles/` — coins, stars
- `entities/` — enemies
- `hazards/` — spikes, saws, flames
- `levels/` — tilesets, platforms
- `player/` — player sprite sheet
- `screens/` — UI elements
- `surfaces/` — bouncepads, bridges, vines, ladders, ropes, rails
