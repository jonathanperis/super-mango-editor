---
title: Optimization Lessons
description: Measure a frame with make bench, then the optimizations we tried, with before and after numbers, and the ones we left out and why.
---

# Optimization Lessons

This page is a record of one round of performance work on the game: how I
measured a frame, what I changed, what it bought, and what I tried and threw
away. The ones I threw away are as useful to read as the ones I kept: most of
them were faster and still not acceptable, because they changed pixels.

Two rules held for every change:

1. **Measure before and after.** A change that cannot show a number does not
   go in.
2. **The picture must not change.** Every render change was checked frame by
   frame against the version before it (see
   [Proving the picture did not change](#proving-the-picture-did-not-change)).

On this page:

- [Running make bench](#running-make-bench)
- [Counting instructions instead of milliseconds](#counting-instructions-instead-of-milliseconds)
- [Where a frame goes](#where-a-frame-goes)
- [Opaque layers without blending](#opaque-layers-without-blending)
- [Drawing only the rows that show: faster, and not shipped](#drawing-only-the-rows-that-show-faster-and-not-shipped)
- [The final copy to the window: also not shipped](#the-final-copy-to-the-window-also-not-shipped)
- [Precompute once: the floor and the HUD](#precompute-once-the-floor-and-the-hud)
- [Draw calls and a texture atlas](#draw-calls-and-a-texture-atlas)
- [The web download](#the-web-download)
- [Why there is no link-time optimization](#why-there-is-no-link-time-optimization)
- [Proving the picture did not change](#proving-the-picture-did-not-change)

## Running make bench

```sh
make bench                 # time every campaign level
make bench BENCH_CHECK=1   # and compare with tools/bench_budget.json
```

`make bench` builds an optimized copy of the game in `out/bench` on raylib's
Memory backend, which draws every pixel on the CPU with its software renderer.
No window or GPU is involved, so it runs the same way on a laptop, in Docker
and on CI. Then `tools/bench.py` plays each level of the campaign for 600
frames (10 seconds) with seed 1 while holding Right, so the camera scrolls and
every run draws the same frames. It runs each level three times and reports the
run with the median frame time:

```text
bench: 600 frames per run, median of 3 runs, per frame:
level                         update ms   draw ms  frame ms  tex switches
levels/00_sandbox_01.toml         0.003    11.605    11.693          24.8
levels/01_lugio_01.toml           0.009    13.140    13.247          23.0
levels/02_lugio_02.toml           0.008    12.503    12.599          21.9
```

- **update ms**: the fixed simulation steps of a frame.
- **draw ms**: drawing, up to handing the frame to the renderer.
- **frame ms**: the whole frame, `EndDrawing` included.
- **tex switches**: how often a draw used a different texture from the draw
  before it. raylib starts a new draw call at each switch, so this is about the
  number of draw calls.

The game measures these itself (`game_frame` in `src/core/game_loop.c`) with
`clock_cpu_seconds()`, the time this thread spent on a CPU. Wall-clock time
grows whenever the OS runs something else; CPU time mostly does not. When a
smoke run ends it prints them on a `SMOKE_TIMING` line, which is what the bench
script reads. Run the game with `--debug` and the same numbers appear as the
second line of the top-right panel, next to the frame interval. With VSync that
interval is mostly the wait for the display, which is why it is not a measure
of work.

`tools/bench_budget.json` holds a budget per level. A level fails only above
three times its budget, because CI machines are slower and noisier than the
Mac the budgets came from; texture switches are the same on every machine, so
their limit is tight. The **Bench** workflow runs `make bench BENCH_CHECK=1` on
every pull request for information only: it is not a required check.

## Counting instructions instead of milliseconds

Milliseconds are noisy. On a busy machine (I had other builds running) the same
binary measured anywhere from 12 to 17 ms a frame. For before-and-after
comparisons I counted instructions instead, with Valgrind's Callgrind in a
Linux container:

```sh
valgrind --tool=callgrind --callgrind-out-file=cg.out \
  out/bench/super-mango --level levels/02_lugio_02.toml --smoke-test-frames 90 \
  --seed 1 --replay-dir out/bench/bench-replays --replay-script move-right
```

I ran each build for 30 and for 90 frames and divided the difference by 60,
which removes the start-up (loading the level). The count barely moves between
runs: about 0.3% here, from the parts of the game that read the clock. The
tables below are instructions per frame on Linux arm64 with clang `-O2`.

## Where a frame goes

On level 02 a frame's update takes about 0.01 ms and drawing takes the rest. A
profile of the drawing (see [Debugging C](../debugging-c/#a-quick-profiling-pass))
put about 55% in `parallax_render`, about 32% in `display_present` (the 400×300
canvas copied to the 800×600 window) and about 9% in the fog. The game logic is
not where the time goes, so everything below is about drawing.

## Opaque layers without blending

Level 02 stacks seven background layers. Each is a 384×216 image stretched to
the full 400×300 canvas, and each is drawn with alpha blending: for every pixel
the renderer reads the colour already there and mixes the new one over it.

The sky is different: every pixel has alpha 255, so the mix always gives the new
colour, and reading the old one is wasted work. `parallax_init_from_def` now
checks each layer's PNG once, at load time (`image_file_is_opaque` in
`src/shared/texture.c`), and `parallax_render` draws an opaque layer with
blending turned off. Blending is a setting of a whole batch, not of one quad,
so the opaque layer flushes what is queued, draws, flushes again and turns
blending back on.

| Level | Before | After | Change |
|---|---:|---:|---:|
| 00 sandbox | 135.7 M | 133.5 M | −1.6% |
| 01 lugio | 144.6 M | 143.5 M | −0.8% |
| 02 lugio | 143.8 M | 143.8 M | 0% (within the noise) |

Less than I expected. The lesson: on this renderer the cost is touching a
pixel at all (finding its texel, converting colours), not the blend. On a GPU,
skipping the read of the old colour saves memory bandwidth, which matters more
on phones; the change is small and safe either way, so it stayed.

I also tried two related ideas and left them out:

- **Skipping layers hidden behind an opaque one.** The only opaque layer in
  every level is the first one, so nothing is ever hidden: no measurable
  effect, so no code.
- **Not clearing the canvas when an opaque layer will cover it.** On the
  software renderer this saved too little to measure. On the tile-based GPUs
  of phones, clearing is what lets the GPU skip loading the old picture into
  its tile memory, so skipping it can make a frame slower there.

## Drawing only the rows that show: faster, and not shipped

Most background layers are transparent over most of their height. The lone
cloud has something to show in 31 of its 216 rows; the cloud bands sit in the
bottom quarter. The renderer still visits every pixel of the full quad. So I
read each layer's alpha at load time, found the first and last row that are not
fully transparent, and drew only that band of the image, stretched to where it
lands on the canvas.

| Level | Before | Visible rows only | Change |
|---|---:|---:|---:|
| 00 sandbox | 135.7 M | 91.6 M | −32.5% |
| 01 lugio | 144.6 M | 103.0 M | −28.8% |
| 02 lugio | 143.8 M | 102.9 M | −28.4% |

On the Mac the frame went from about 12.5 ms to 8.6 ms. By far the biggest win
of the round, and it is not in the game, because it changed the picture:
about 0.5% of the pixels of each frame came out different.

Here is why. A layer is 216 texel rows stretched over 300 pixel rows, so each
pixel row has to pick one texel row, and on some pixel rows the exact answer
lies precisely on the border between two texel rows. The renderer computes the
texture coordinate of each row by adding a small step to the one before, and
those additions round. On a border row, a rounding error of the last bit
decides which texel wins. A shorter quad starts its additions at a different
row with a different step, so it rounds differently and some border rows pick
the neighbouring texel. Neither picture is wrong, but they are not the same
picture. (A GPU has the same kind of border rows, only at different places.)

I could not find a way to draw a shorter quad that rounds exactly like the full
one without copying the renderer's arithmetic, which would break with the next
raylib. If one pixel in two hundred moving to its neighbouring texel at a
texel border is acceptable, the change is about 40 lines (measure the band in
`parallax_init_from_def`, draw it with `DrawTexturePro` and a source rectangle
of just those rows) and gives back a third of the frame. The fog has the same shape (images stretched
from 320×180 to 800×300, never opaque), so it got neither change.

## The final copy to the window: also not shipped

`display_present()` copies the finished canvas into the window. The canvas
covers the window, so blending looked like wasted work again. Turning it off
saved 8–9% of the frame:

| Level | Before | Copy without blending | Change |
|---|---:|---:|---:|
| 00 sandbox | 132.6 M | 120.9 M | −8.8% |
| 02 lugio | 143.7 M | 132.1 M | −8.1% |

and it changed pixels in every level with fog, and on every frame with the
pause overlay. raylib's alpha blending also blends the alpha channel, so a
canvas pixel under something translucent ends up with an alpha below 1. The
blended copy over the black window then darkens it a little; the unblended
copy did not. The darkening is arguably a bug, but fixing it is a visual change
to decide on its own, not a side effect of an optimization. Blending stays on,
with a comment in `display_present()` that says why.

## Precompute once: the floor and the HUD

Two small cases of the same idea: if an answer does not change, do not work it
out again.

**The floor** (`render_floor` in `src/render/game_render.c`) is three rows of
16-pixel pieces. For every piece it checked every floor gap twice: once for
"is this piece inside a gap?" and once for "is it beside one, so it needs an
edge cap?". But the answer depends only on the column, not on the row, so it
was worked out three times. Now `floor_piece_column()` answers both questions
in one pass over the gaps, once per visible column, and the row loop reads the
result.

**The HUD** (`hud_render` in `src/screens/hud.c`) formatted the lives and the
score with `snprintf` and measured them with `MeasureTextEx` (which looks up
every glyph) 60 times a second, for numbers that change a few times a minute.
Now the HUD keeps the last layout with the three numbers it was made for, and
lays out again only when one of them differs.

| Level | Before | After both | Change |
|---|---:|---:|---:|
| 00 sandbox | 133.5 M | 132.6 M | −0.6% |
| 01, 02 lugio | | | within the 0.3% noise |

These are lessons, not wins: a few thousand instructions in a frame of 140
million. The useful habit is spotting a loop that recomputes something its
inputs do not change, and the measurement tells you when it is not worth more
than a comment.

## Draw calls and a texture atlas

Every kind of thing (each background layer, the floor, coins, each enemy kind,
the HUD font) has its own texture, and raylib starts a new draw call whenever
the texture changes. A texture atlas packs many images into one texture so that
consecutive draws do not switch. Before building one I counted: the texture
switches counter (`texture_note_use` in `src/shared/texture.c`, shown by the
debug overlay and by `make bench`) reads 22–25 per frame.

Then I measured what one draw call costs, by forcing one per sprite: level 02
went from 22 to 175 draw calls per frame and from 143.8 M to 144.3 M
instructions, about 3,250 instructions per extra draw call. An atlas that took
the 22 calls down to 6 would save some 50,000 instructions, 0.04% of the frame.
On a GPU, a few dozen draw calls a frame are also far below where they start to
matter. So there is no atlas: it would make the sprite code harder to read for
a saving nobody could see. The counter stays, so the day a level has hundreds of
switches someone will notice.

## The web download

Before the game starts, the page downloads `super-mango.wasm`, `super-mango.js`
and `super-mango.data` (every asset and level). Sizes in bytes, raw and as
a server sends them with Brotli:

| File | Before | After | Brotli before | Brotli after |
|---|---:|---:|---:|---:|
| `super-mango.wasm` | 369,437 | 371,404 | 142,379 | 142,974 |
| `super-mango.js` | 184,968 | 89,915 | 40,048 | 26,920 |
| `super-mango.data` | 317,695 | 246,287 | 272,922 | 201,873 |
| total | 872,100 | 707,606 | 455,349 | 371,767 |

Two changes made that −18% (compressed):

- **Music no level plays stays out.** `winds.qoa` (71 KB, 22% of the data
  file) is offered by the editor but played by no level, and the browser
  build has no editor. The Makefile reads every level's `music_path` and
  passes the tracks nobody names to emcc as `--exclude-file`, and
  `tools/check_wasm_artifacts.py` fails the build if a track a level plays
  ever goes missing. A level that starts using `winds.qoa` brings it back
  with no other change.
- **The Closure Compiler** (`--closure 1`) shortens every name in the
  JavaScript emcc writes, which halved `super-mango.js`. Closure renames
  properties too, and some properties are shared with `web/boot.js`, the
  page's own script that Closure never sees (`Module.onGameEnded`,
  `window.SuperMangoTouch`, ...). A renamed one fails quietly: the game simply
  never calls it. `web/closure-externs.js` lists those names, and `make
  web-host-contract` fails when the game uses a shared name the list lacks. I
  also checked the built game in headless Chromium against a build without
  Closure: the same host objects exist, the touch export answers, a settings
  change writes the profile to `localStorage`, holding Right moves Mango, and
  the console shows the same messages.

The wasm grew by 2 KB: the CPU timing code and the opacity check above.
`tools/check_wasm_artifacts.py` now also has a gzip budget for each of the
three files (the size when it was set plus about 10%), and `make asset-budget`
allows 320 KiB of raw assets instead of 4 MiB; the assets are 284 KiB.

## Why there is no link-time optimization

Link-time optimization (`-flto`) lets the compiler optimize across source
files. Measured on this game it made the native release binary 2.8% smaller
and the web build 0.6% smaller. Most of
the binary is raylib, which `tools/build_raylib.py` builds without `-flto`,
so LTO only reaches the game's own code. A second reason: release builds would
then differ more from the debug builds the tests run. Not worth it for 2.8%.

## Proving the picture did not change

For every render change I built the game before and after it on the Memory
backend with a small hook (not in the repository) that reads the window's
pixels after every frame and prints a hash of them. Each of the 9 levels (the
campaign and the six labs) ran three scripted inputs for 600 frames: walking
right, running and jumping, and pausing. That is 16,200 frames per build, and
every hash of the final build matches the hash of the same frame before this
round. The two changes above that made any hash differ are the two that are not
in the game.
