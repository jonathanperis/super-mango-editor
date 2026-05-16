# Overlay Text Snapshots

Generated/checked by `tools/check_roadmap_quality.py` against `src/render/render_overlay.c`.

These text snapshots keep the user-facing overlay copy auditable even when pixel
screenshots are not available in headless CI. If overlay labels intentionally
change, update this page with the same PR that changes the renderer.

## Pause overlay

- `Paused`
- `Enter/Space/Esc/Start: resume`
- `Close window to quit`

## Game-over overlay

- `Game Over`
- `Final Score: <score>`
- `Enter/Space/Start: restart`
- `Esc/Back: exit`

## Level-completion overlay

- `Level Complete!`
- `Game Complete!`
- `Score: <score>`
- `Coins: <collected>/<total>`
- `Lives: <lives>`
- `Time: <mm:ss>`
- `Congratulations!`
- `Enter/Space/Start: next level`
- `Enter/Space/Start: finish`
- `Esc/Back: exit`
