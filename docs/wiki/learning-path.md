# Sandbox School: Start Here

Super Mango is a working C11/raylib game you can take apart. Learn one mechanism at
a time, then combine them in the playground. You need basic C expressions,
functions, structs and pointers; raylib experience is optional.

## First successful build

Install the dependencies in the repository README. Python **3.11+** and Node.js
are also needed for the full verification tools. Linux editor dialogs use
`zenity`. From the repository root:

```sh
make builder CC=clang
make run-editor CC=clang
make run-level-debug LEVEL=levels/labs/01_collision.toml
```

`builder` builds the game and editor together. Open a lab in the editor, use
**Save As** to create your own copy, and press **F5** to playtest. Playtests use
`--no-save`, so experiments do not change your personal game profile. Use
`make debug` for debugger symbols in `out/debug/`, or `make release` for optimized
executables in `out/release/`. Keep runtime working directories at the repository
root (or the extracted release directory) so asset paths resolve.

## How to use each lab

Before changing anything, predict the result. Make one change, observe it, and
explain any difference from your prediction. Record the command, seed and result.
Use [the mechanics museum](../mechanics-museum/) for compact examples and
[Controls](../controls/) for inspection keys.

### Lab 1 — Your first frame

- **Goal:** distinguish world, logical-screen and window coordinates.
- **Read:** `src/main.c`, `src/core/game_loop.c`, `src/render/game_render.c` and `src/collectibles/coin.c`.
- **Run:** `make run-level-debug LEVEL=levels/labs/01_collision.toml`.
- **Change:** move its coin 32 logical pixels right in a copied TOML file.
- **Observe:** the coin moves relative to the platform; changing window scale does not change its world coordinates.
- **Proof:** explain why rendering subtracts `camera.x`, but the stored coin position does not. Validate your copied level through the editor before playtesting.

### Lab 2 — Motion and numerical integration

- **Goal:** understand acceleration, friction and timestep error.
- **Read:** `src/player/player_motion.c`, `src/player/player.c`, `src/core/game_timing.c`.
- **Run:** `make run-level-debug LEVEL=levels/labs/06_camera.toml` and `make timing-lab`.
- **Change:** select `ground_friction` with F6; use minus/equal to adjust it. F7 restores authored/default values.
- **Observe:** releasing movement changes stopping distance. F4 slows simulated time; F2 freezes and F3 advances one 1/60-second step.
- **Proof:** explain why multiplying by dt gives units of distance but does not eliminate numerical error. The timing lab compares 30/60/144 render rates and a fixed simulation step.

### Lab 3 — One-way collisions

- **Goal:** understand a crossing test rather than only overlap.
- **Read:** `src/player/player_surfaces.c` and `tests/session_test.c` (`nearest_surface_is_order_independent`).
- **Run:** `make run-level-debug LEVEL=levels/labs/01_collision.toml`.
- **Change:** lower the second ledge in an editor copy; jump through it and land.
- **Observe:** the stored previous foot position determines whether a descending player crossed a surface. The cyan/green foot marker makes that point visible.
- **Proof:** the player passes upward through a ledge and lands downward. Run `make test` after changing collision code; explain why a nearer surface must win regardless of array order.

### Lab 4 — State machines

- **Goal:** follow transitions between waiting, moving and damaging states.
- **Read:** `src/hazards/blue_flame.c`, `src/entities/fish.c`, `src/core/game_hazards.c` and `src/collision/game_collision.c`.
- **Run:** `make run-level-debug LEVEL=levels/labs/05_hazards.toml`.
- **Change:** change one flame duration constant; rebuild and compare against the original.
- **Observe:** freeze/step the active hitbox, and compare it with the hurt-immunity timer after a hit.
- **Proof:** explain why collision uses the hazard's updated position, and why a waiting flame must not damage the player. `make test` exercises the same-frame saw boundary.

### Lab 5 — Data-driven design

- **Goal:** trace TOML → validated placement → live entity → editor round-trip.
- **Read:** the [entity walkthrough](../entity-walkthrough/), `src/shared/serializer_parse.c`, `src/shared/serializer_load_collectibles.c` and `src/levels/level_loader.c`.
- **Run:** `make run-editor`; open a copied collision lab.
- **Change:** place a coin, change its coordinates, save, close, reopen, then playtest.
- **Observe:** the saved values survive and match the runtime position.
- **Proof:** `make test` checks round-trips and invalid inputs; a fractional integer, NaN or over-capacity array must fail without replacing the active document.

### Lab 6 — Ownership and failure

- **Goal:** distinguish an owning pointer, a borrowed pointer and a failed construction.
- **Read:** `src/core/game_lifecycle.c`, `src/core/game_resources.c`, `src/shared/serializer_io.c`.
- **Run:** `make sanitize CC=clang`.
- **Experiment:** read the simulated missing-saw-texture scenario in `tests/simulation_test.c`; it temporarily clears an in-memory texture slot and restores ownership before cleanup. Do not delete shared assets to perform this exercise.
- **Observe:** required gameplay assets fail clearly; the error identifies the path. Failed save/load operations preserve existing data.
- **Proof:** explain why `if (pointer) free(pointer)` alone does not prevent a second free, and why an owner clears its pointer after release.

### Lab 7 — Editor commands

- **Goal:** understand reversible operations and the distinction between history and document state.
- **Read:** `src/editor/undo.c`, `src/editor/editor_undo_apply.c`, `src/editor/editor_session.c`.
- **Run:** `make run-editor`.
- **Change:** move a coin, edit the level description, undo both, redo one, then make another edit.
- **Observe:** undo restores values; the new edit invalidates redo; returning to the saved contents removes the dirty marker.
- **Proof:** run `make test`. Explain why entity commands use inline values while configuration commands own separately allocated snapshot pairs, and how ownership transfers between stacks.

### Lab 8 — Reproducible experiments and portability

- **Goal:** separate semantic input and simulation time from keyboard layout and rendering.
- **Read:** `src/input/game_web_input.c`, `web/touch-controls.js`, `src/core/game_experiment.c`, `src/core/game_random.c`.
- **Run:** `./out/super-mango --debug --no-save --seed 7 --level levels/labs/06_camera.toml`.
- **Experiment:** press F8 to restart and record; move, jump and change a tuning field; press F9 to export. Native saves a uniquely named TOML file in the working directory; the browser initiates an explicit download.
- **Replay:** `./out/super-mango --level levels/labs/06_camera.toml --experiment mango-experiment-N.toml` (replace `N` with the exported filename).
- **Proof:** the replay freezes after the captured simulation steps. `make test` compares position, velocity, elapsed time, score and checkpoint against the recorded run despite opposing live input. Use unchanged level bytes and the same engine revision. Floating-point/platform differences can still prevent bit-identical cross-platform results.

## Completion portfolio

Keep a small learning log: prediction, changed file/property, run command, observed
result and explanation. A useful milestone is being able to add a collectible,
show its round-trip and undo behavior, and share a reproducible experiment. That
is more informative than counting how many documentation pages you opened.
