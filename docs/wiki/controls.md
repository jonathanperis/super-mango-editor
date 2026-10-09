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

The mouse can also click **Play** and the **Settings (F1 / Y)** button. The selected level wraps at either end of the manifest-defined catalog. A held confirm carried from a prior screen must be released before it can start the selected level. A missing or malformed manifest, or one with no playable level, prevents the native menu from opening. A single listed level that fails to load stays in the selector, greyed out with an "Unavailable" reason, and cannot be started; the rest remain playable (see [Campaign Manifest](../level-design/#campaign-manifest-v1)).

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
- **Navigate:** Up/Down or D-pad selects a row, wrapping at the ends; Left/Right changes values on the main page; Enter, keypad Enter, Space or A/Start activates, and a mouse click activates a row. Esc or B/Back closes, or cancels binding capture first; either one also clears the message about a refused binding. The start menu's **Settings** button opens the panel exactly as F1 does.
- **Options:** music/effects volume (0–128 in steps of 8), mute, stick dead zone (0–28000 in steps of 1000), native window scale (1×–4×; not applied in the browser), high-contrast outlines, reduced motion, control remapping (a separate page of 12 binding rows) and **Restore default settings**, which resets every setting, not only controls.
- **Remapping:** Left, Right, Up, Down, Jump and Run each have keyboard and gamepad bindings. Duplicates are rejected, as are reserved keys (Esc, F1, Tab, Enter, keypad Enter, the arrows and Right Shift) and buttons (Back, Start, Guide, B); misc, paddle and touchpad buttons cannot be captured. Arrows remain available. In debug gameplay, F2–F10 and `-`/`=` are reserved for the inspector.

Settings apply when the panel closes. Normal runs save settings, the last played
stage and per-level best score/time/coin results. A profile holds results for up
to 128 levels; a coin result is at most `MAX_COINS` (64), the most coins one level
can place, so raising that constant keeps old profiles readable. A result that
cannot be recorded (a full profile) is logged as a warning rather than dropped
silently. `--continue` opens that stage
from its start, not a mid-level checkpoint; all campaign levels remain selectable.
Native profiles use `profile.toml` under the OS preference root plus `SuperMango/SuperMango/`,
or an explicit `--profile PATH`. Browser profiles use localStorage and Web Locks;
unsupported/denied storage or conflicting saves produce a visible profile warning
(F1 shows details). Exit/replay waits for a pending save to settle. Debug, smoke,
scripted replay, experiment replay and `--no-save` runs do not read or write the
personal profile. Editor playtests pass `--no-save`.

## Browser / WebAssembly Input

The game on the GitHub Pages site uses the WebAssembly build generated by `make web`. Its normal and debug launchers deliberately pass `--level levels/00_sandbox_01.toml`, so they boot that TOML level directly rather than showing the native campaign selector; Debug Mode also adds `--debug`. Keyboard controls match the native build. A mapped controller follows the same action route when the browser exposes one, but keyboard remains the dependable browser control path.

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

`--debug` (or `make run-level-debug LEVEL=...`) opens the simulation inspector
and turns off saving to your personal profile. The game always advances in
fixed 1/60 s steps (`src/core/game_timing.c` keeps an accumulator of real
time), so the inspector controls *how many* steps run, never how long one is.

### Debug inspector keys

This is the one reference table for the inspector; F5 shows the same list in
the game. The keys are handled in `src/core/game_inspector.c`.

| Key | What it does |
|-----|--------------|
| F2 | Freeze or resume the simulation |
| F3 | Freeze and advance exactly one 1/60 s step (also during replay) |
| F4 | Cycle speed: 1×, 0.25×, 0.1× (fewer steps per second, each still 1/60 s) |
| F5 | Open or close the key list in the game |
| F6 | Select the next of the nine movement values |
| `-` / `=` | Lower or raise the selected value by 25, between zero and `MAX_LEVEL_MOTION` |
| F7 | Restore the level's own movement values |
| F8 | Restart the level from its seed and start recording |
| F9 | Export the recording |
| F10 | Choose what the detail row inspects: the player, fish, float platforms or saws in the level |

The nine movement values are walk/run max speed, walk/run ground acceleration,
ground friction, ground counter-acceleration, walk/run air acceleration and air
friction. While a recording is replaying, F4, F7 and `-`/`=` are ignored so the
run cannot change; F2 and F3 still pause and step it. In debug gameplay these
keys are reserved and cannot be used as remapped controls. Focus loss, the
settings panel, your own pause and the end-of-level and game-over screens
still stop the simulation; a step requested while it is stopped is dropped,
not saved for later.

### What the overlay shows

Besides hitboxes, patrol lines and the velocity arrow, the overlay keeps its
text in a few small panels: a status line at the top left (`LIVE`, `FROZEN` or
`SLOW`, plus `REC n` or `REPLAY n/m`), one performance line at the top right
(FPS, frame time, memory), one player line at the bottom right (state,
ground/air, facing, velocity, and the checkpoint, riding platform and hurt time
when set) and recent events at the bottom left. The tuned movement value
appears under the status line for three seconds after F6/F7/`-`/`=`, and stays
while it differs from the level's value; the F10 selection appears there while
it is not the player.

### Recording and replaying an experiment

F8 restarts the current level and records, for every fixed 1/60 s step, the
inputs (as actions like left or jump, not physical keys) and the nine movement
values, plus the seed. It records at most 36,000 steps (10 minutes) and refuses
to start if the level file changed since it was loaded; if you edit the level
in another program, reopen the game first. A recording belongs to one level:
going to another level, or choosing Replay, Level Select or Exit, throws it
away, so export it before you leave.

F9 exports the recording, including from the completion and game-over screens,
as `mango-experiment-<milliseconds>.toml` in the working directory (or as a
browser download). It never overwrites an existing file. The file
(`format_version = 2`) has one row per step, `[input bits, nine movement
values]`, plus the level path, the seed and `level_hash`, a hash of the level
file's exact bytes.

Replay it with `--level <same level> --experiment <file>`. The game checks the
format version, the seed, the 1–36,000 step count and that the level file
still hashes to `level_hash`, then plays the recorded inputs, ignores live
movement and freezes after the last step. A changed level is rejected rather
than silently producing a different run. So is a `format_version = 1` file:
it came from the earlier variable-timestep engine and cannot be replayed
faithfully, so record it again. The engine version is not checked, so replay
with the same build. Recordings are not save games: they hold no pause time,
audio or pixels. See [Mechanics Museum](../mechanics-museum/) for levels to
try it on.

## Runtime Flags for Input and CI

| Flag | Use |
|------|-----|
| `--help` | Prints usage and exits. |
| `--debug` | Enables the inspector, FPS/frame interval, memory, hitboxes and event log; disables personal-profile persistence. |
| `--sandbox` | Loads `levels/00_sandbox_01.toml` directly (whichever of `--sandbox`/`--level` comes last wins). |
| `--level <path>` | Starts gameplay from a specific TOML file and skips the start menu. |
| `--continue` | Opens the last saved stage if available and no explicit level was supplied; otherwise, or when that stage no longer loads, opens the selector. Runs that do not read the profile (`--no-save`, `--debug`, smoke, replay scripts) always open the selector. |
| `--profile <path>` | Uses an explicit native profile file. |
| `--no-save` | Keeps settings/results in memory only; does not read or write the personal profile. |
| `--experiment <path>` | Replays an exported capture; requires `--level` (whose bytes must match the capture's `level_hash`), enables debug/no-save, and cannot combine with `--replay-script`. |
| `--smoke-test-frames N` | Runs exactly `N` (positive) frames and exits for bounded CI smoke checks; defaults to `levels/00_sandbox_01.toml` without `--level` and disables the profile. |
| `--seed N` | Seeds the explicit unsigned PRNG for reproducible enemy timers and experiments; without it the seed comes from a monotonic clock. |
| `--replay-script <name>` | Injects `<name>.replay` from the replay folder (`out/replays-smoke/` unless `--replay-dir` says otherwise) as deterministic commands and action masks. Only `move-right`, `jump-right` and `pause-resume` are accepted; disables the profile. |
| `--replay-dir <dir>` | Reads `--replay-script` files from `<dir>` instead of `out/replays-smoke/`; requires `--replay-script`. `make scripted-smoke` passes `$(OUTDIR)/replays-smoke`. |

An unknown option, or a value flag whose value is missing or starts with `-`, exits with an error.

**Starting from another folder.** The game and editor read `assets/` and
`levels/` relative to the working folder. When that folder does not hold them
(a desktop shortcut, a file manager, `../out/super-mango`), both programs move
to the executable's folder, or one or two folders above it, before loading
anything. `--profile`, `--experiment`, `--replay-dir` and the editor's document
path are read relative to the folder you started in. `--level` is too when the
file exists there; otherwise it names a bundled level such as
`levels/labs/01_collision.toml`. F9 experiment exports then land in the
asset folder.

Deterministic replay scripts are generated by `tools/run_scripted_smoke.py` and consumed by `src/input/game_replay.c`. They are CI test fixtures, distinct from the player-facing Browser Replay action.

## Related Pages

- [Build System](../build-system/) — Make targets and CI smoke commands.
- [Player Module](../player-module/) — movement, physics, animation, hitbox, and climb logic.
- [Level Design](../level-design/) — TOML schema and per-level physics overrides.
