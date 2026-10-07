# Controls & Input

<a id="home"></a>

---

Super Mango accepts keyboard input, raylib-mapped gamepads, browser touch controls, and deterministic replay input. The tables below describe default bindings; F1 opens settings and control remapping.

## Player Controls

| Action | Keyboard | Desktop gamepad | Notes |
|--------|----------|-----------------|-------|
| Move left / right | `A` / `D` or Left / Right arrows | D-Pad Left / Right or left stick X | The player accelerates toward the target speed instead of snapping instantly. |
| Run | Left Shift (remappable); Right Shift always runs | Right bumper / R1 | Run changes max speed and uses a lower-air-control jump arc once airborne. |
| Jump | Space | A / Cross | Fresh presses are buffered briefly before landing; releasing jump early cuts the jump short. |
| Grab / climb up | `W` or Up arrow | D-Pad Up or left stick Up | While overlapping a vine, ladder, or rope. |
| Climb down | `S` or Down arrow | D-Pad Down or left stick Down | Only while attached to a climbable surface. |
| Drift while climbing | `A` / `D` or Left / Right arrows | D-Pad Left / Right or left stick X | Uses reduced climb drift speed. |
| Jump off climbable | Space | A / Cross | Leaves the climbable and starts a normal jump. |

The gamepad path translates raylib's standardized buttons to project action IDs. The analog stick defaults to an 8000-unit dead zone, adjustable in settings; raylib's normalized axes are converted to the existing saved units. Connection changes are sampled each frame on the application thread.

Existing profile binding numbers are preserved. Media keys, extra paddles or
touchpad buttons unavailable through raylib retain their saved values and show
an unavailable/remap warning. Fixed arrows and navigation controls remain
reachable; new captures accept only supported inputs.

## Start Menu and Level Select

Without `--level`, the native executable loads `levels/campaigns/main.toml`: Creator's Playground, then the two Volcanic Depths stages. The selector reads each TOML `name` (falling back to the filename). **Level Select** closes the active game screen and reopens that catalog in the same session. Separate mechanics examples use `--level levels/labs/NAME.toml`.

`--level <path>` and `--sandbox` start gameplay directly and skip the selector. `--level` has no campaign-membership check, so a valid TOML level may be launched even when it is not listed in the manifest. The current catalog is documented in the generated [Level Catalog](../level-catalog/).

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| Previous level | Left, A, or Up | D-pad Left or Up |
| Next level | Right, D, or Down | D-pad Right or Down |
| Play selected level | Enter, keypad Enter or Space | A / Cross or Start |
| Exit menu | Esc | B / Circle or Back |

The mouse can also click **Play** and the **Settings (F1 / Y)** button. The selected level wraps at either end of the manifest-defined catalog. A held confirm carried from a prior screen must be released before it can start the selected level. A missing or invalid manifest prevents the native menu from opening; fix the manifest or its listed TOML files rather than expecting a fallback selector.

## Pause and Terminal Overlays

| State | Keyboard | Gamepad | Behaviour |
|-------|----------|---------|-----------|
| Active gameplay | Esc | Start | Toggle player pause. |
| Pause overlay | Enter, keypad Enter or Space; Esc toggles | A / Cross; Start toggles | Clear player pause. Esc and Start toggle the player-pause bit, so during a focus-only pause they add a player pause. A focus pause still waits for focus to return; window close exits. Back opens settings; B has no pause action. |
| Terminal overlays | Up or W / Down or S | D-pad Up / Down | Move focus with wraparound. |
| Terminal overlays | Enter, keypad Enter or Space | A / Cross or Start | Confirm focused action. |
| Terminal overlays | Esc | B / Circle or Back | Exit the run immediately; does not confirm the focused action. |

| Overlay | Actions, in focus order |
|---------|-------------------------|
| Completion with `next_phase` | Next Level, Replay, Level Select, Exit |
| Final completion | Replay, Level Select, Exit |
| Game over | Retry, Level Select, Exit |

**Next Level** loads the resolved `next_phase` in the current game session. A failed load keeps the completion overlay, shows "Next level failed to load", removes the Next Level row and focuses Replay; Replay, Level Select and Exit keep working. **Retry** restarts the current level in place with level-defined hearts and lives, score reset, and music resumed. **Level Select** returns to the start menu. **Exit** ends the application session.

**Release latch.** After a pause resumes, settings close, Retry or Next Level,
the game records every bound key and button still held (a gamepad A/Start that
confirmed counts too). Gameplay input reads as zero until all of them are
released, so the Space or A that resumed a pause does not also jump.

The overlay text is snapshotted in [Overlay Snapshots](../overlay-snapshots/) so docs drift checks catch stale copy and control hints.

## Settings and Saved Progress

- **Open:** F1 on either screen; gamepad Y in the start menu or Back during gameplay/pause. On terminal overlays, Back exits; use F1 for settings.
- **Navigate:** Up/Down or D-pad selects a row, wrapping at the ends; Left/Right changes values on the main page; Enter, keypad Enter, Space or A/Start activates, and a mouse click activates a row. Esc or B/Back closes, or cancels binding capture first.
- **Options:** music/effects volume (0–128 in steps of 8), mute, stick dead zone (0–28000 in steps of 1000), native window scale (1×–4×; not applied in the browser), high-contrast outlines, reduced motion, control remapping (a separate page of 12 binding rows) and **Restore default settings**, which resets every setting, not only controls.
- **Remapping:** Left, Right, Up, Down, Jump and Run each have keyboard and gamepad bindings. Duplicates are rejected, as are reserved keys (Esc, F1, Tab, Enter, keypad Enter, the arrows and Right Shift) and buttons (Back, Start, Guide, B); misc, paddle and touchpad buttons cannot be captured. Arrows remain available. In debug gameplay, F2–F10 and `-`/`=` are reserved for the inspector.

Settings apply when the panel closes. Normal runs save settings, the last played
stage and per-level best score/time/coin results. `--continue` opens that stage
from its start, not a mid-level checkpoint; all campaign levels remain selectable.
Native profiles use `profile.toml` under the OS preference root plus `SuperMango/SuperMango/`,
or an explicit `--profile PATH`. Browser profiles use localStorage and Web Locks;
unsupported/denied storage or conflicting saves produce a visible profile warning
(F1 shows details). Exit/replay waits for a pending save to settle. Debug, smoke,
scripted replay, experiment replay and `--no-save` runs do not read or write the
personal profile. Editor playtests pass `--no-save`.

## Browser / WebAssembly Input

The GitHub Pages cabinet uses the WebAssembly build generated by `make web`. Its normal and debug launchers deliberately pass `--level levels/00_sandbox_01.toml`, so they boot that TOML level directly rather than showing the native campaign selector; Debug Mode also adds `--debug`. Keyboard controls match the native build. A mapped controller follows the same action route when the browser exposes one, but keyboard remains the dependable browser control path.

The pinned Emscripten GLFW keyboard handlers are scoped to `Module.canvas` by `web/keyboard-scope.js`. Canvas blur releases backend keys; screen transitions clear queued commands/touch holds and release-latch inherited physical input. Held controls must be released before they affect a new screen. Tab and keyboard interaction outside the game remain owned by the page.

Tab leaves the game canvas. Keyboard browser shortcuts remain available. The
embedded and standalone hosts provide touch buttons for Left/Right/Up/Down,
Jump/Confirm, Run, Pause/Back and Settings. Multiple pointers can hold movement
and jump together; cancellation, focus loss and screen changes clear held input.
Touch actions remain independent of remapped keyboard keys and are disabled
until the runtime starts and after it exits.

### Browser Replay

Selecting **Replay** on a completion overlay is deliberately a browser reload, not an in-place game replacement. The session stores the current TOML path under `super-mango-replay-level` in `sessionStorage`, cancels its Emscripten callback, closes the game and runtime once, then reloads the page. The Pages home page consumes and removes that one-shot value and starts a normal run with `--level <stored path>`; the standalone `super-mango.html` shell forwards it only when it matches a bundled path (`levels/NAME.toml` or `levels/labs/NAME.toml`). Replay resumes the same level in normal mode. If session storage fails, the completion overlay remains active, no reload occurs, and the runtime logs `Replay unavailable: storage failed`. If an earlier profile save failed and changes are unsaved, Replay is refused with `Replay blocked: profile not saved. Use Level Select or Exit.`

## Debug Experiments

`--debug` opens the simulation inspector and disables personal-profile persistence.
Besides hitboxes, patrol lines and the velocity arrow, the overlay keeps its text
in a few small panels: a status line at the top left (`LIVE`, `FROZEN` or `SLOW`,
plus `REC n` or `REPLAY n/m`), one performance line at the top right (FPS, frame
time, memory), one player line at the bottom right (state, ground/air, facing,
velocity, and the checkpoint, riding platform and hurt time when set) and recent
events at the bottom left. The tuned movement value appears under the status line
for three seconds after F6/F7/minus/equal, and stays while it differs from the
level's value; the F10 selection appears there while it is not the player.
F2 freezes/resumes; F3 advances one 1/60-second step (also during replay);
F4 cycles 1×, 0.25× and 0.1× speed. F6 cycles nine movement fields (walk/run max
speed, walk/run ground acceleration, ground friction, ground counter-acceleration,
walk/run air acceleration, air friction); minus/equal adjusts the selected field
by 25 within its limits, and F7 restores authored values. F10 cycles
player/fish/platform/saw inspection. F5 opens and closes a key-help table. During experiment replay, F4,
F7 and minus/equal are ignored. Focus/settings/player pause and terminal screens
retain priority.

F8 restarts the current level and records, for every fixed 1/60 s step, the
semantic inputs and movement tuning, plus the seed (capture `format_version = 2`;
older version-1 captures from the variable-timestep engine are refused). F8 refuses
to start if the level file changed since it was loaded. F9 explicitly exports the
capture, including from completion and game-over overlays, as
`mango-experiment-<milliseconds>.toml` in the working directory (or a browser
download), preserving existing files. Replay checks the format version, the seed,
1–36,000 recorded steps and that the `--level` file's bytes hash to the capture's
`level_hash`; it does not check the engine revision, so replay with the same
build. It ignores live movement and freezes at the last recorded step. Captures are bounded to 36,000 simulation steps and do not
span level transitions. See [Mechanics Museum](../mechanics-museum/).

## Runtime Flags for Input and CI

| Flag | Use |
|------|-----|
| `--help` | Prints usage and exits. |
| `--debug` | Enables the inspector, FPS/frame interval, memory, hitboxes and event log; disables personal-profile persistence. |
| `--sandbox` | Loads `levels/00_sandbox_01.toml` directly (whichever of `--sandbox`/`--level` comes last wins). |
| `--level <path>` | Starts gameplay from a specific TOML file and skips the start menu. |
| `--continue` | Opens the last saved stage if available and no explicit level was supplied; otherwise opens the selector. Runs that do not read the profile (`--no-save`, `--debug`, smoke, replay scripts) always open the selector. |
| `--profile <path>` | Uses an explicit native profile file. |
| `--no-save` | Keeps settings/results in memory only; does not read or write the personal profile. |
| `--experiment <path>` | Replays an exported capture; requires `--level` (whose bytes must match the capture's `level_hash`), enables debug/no-save, and cannot combine with `--replay-script`. |
| `--smoke-test-frames N` | Runs exactly `N` (positive) frames and exits for bounded CI smoke checks; defaults to `levels/00_sandbox_01.toml` without `--level` and disables the profile. |
| `--seed N` | Seeds the explicit unsigned PRNG for reproducible enemy timers and experiments; without it the seed comes from a monotonic clock. |
| `--replay-script <name>` | Injects `out/replays-smoke/<name>.replay` as deterministic commands and action masks. Only `move-right`, `jump-right` and `pause-resume` are accepted; disables the profile. |

An unknown option, or a value flag whose value is missing or starts with `-`, exits with an error.

Deterministic replay scripts are generated by `tools/run_scripted_smoke.py` and consumed by `src/input/game_replay.c`. They are CI test fixtures, distinct from the player-facing Browser Replay action.

## Related Pages

- [Build System](../build-system/) — Make targets and CI smoke commands.
- [Player Module](../player-module/) — movement, physics, animation, hitbox, and climb logic.
- [Level Design](../level-design/) — TOML schema and per-level physics overrides.
