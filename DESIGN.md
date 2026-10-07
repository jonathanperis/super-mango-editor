# DESIGN.md

## Current direction (2026-10)

The site is built from the game itself. The home page opens on a small level
made of the real sprites (sky, mountains, clouds, grass drawn with the same
9-slice the game uses, the walking hero, coins, a spider, a bird) with the
start-menu logo as the title. Text sits in game-style boxes: thick black edges,
an inner frame and a solid offset shadow instead of glows. The palette comes
from the art (night blues, leaf green and mango red from the logo, coin gold for
actions); Pixelify Sans is for headings and buttons, Atkinson Hyperlegible for
reading, DM Mono only for code. Copy is first-person and specific: what the
game is, why it was written, what you can learn, without status-panel jargon.
The "learn" section shows a real excerpt of `src/core/game_loop.c`, read at
build time so it cannot go stale. The manual shares the same tokens, and each
manual category carries a small sprite (Mango, a saw blade, a spider, a star).
The 404 is a tiny scene of its own: Mango walks off a ledge into the game's
water strip, with the real floor-gap rule from the C source underneath.

The colours, fonts and shadow live as CSS variables in
`docs/src/styles/globals.css`. The manual pages are `docs/wiki/*.md`; their
order, categories and descriptions come from `docs/src/lib/docsSidebar.ts`.

## History

An earlier, much longer version of this file planned an arcade-cabinet
redesign of the site and still described the game's old SDL2 build. The
direction above replaced it. The old plan is kept in git history instead of
here; read it with `git log -p -- DESIGN.md`.
