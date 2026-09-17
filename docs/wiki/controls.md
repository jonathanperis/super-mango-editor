# Controls & Input

<a id="home"></a>

---

Super Mango accepts keyboard input, mapped SDL gamepads, browser touch controls, and deterministic replay input. The tables below describe default bindings; F1 opens settings and control remapping.

## Player Controls

| Action | Keyboard | Desktop gamepad | Notes |
|--------|----------|-----------------|-------|
| Move left / right | `A` / `D` or Left / Right arrows | D-Pad Left / Right or left stick X | The player accelerates toward the target speed instead of snapping instantly. |
| Run | Left Shift or Right Shift | Right bumper / R1 | Run changes max speed and uses a lower-air-control jump arc once airborne. |
| Jump | Space | A / Cross | Fresh presses are buffered briefly before landing; releasing jump early cuts the jump short. |
| Grab / climb up | `W` or Up arrow | D-Pad Up or left stick Up | While overlapping a vine, ladder, or rope. |
| Climb down | `S` or Down arrow | D-Pad Down or left stick Down | Only while attached to a climbable surface. |
| Drift while climbing | `A` / `D` or Left / Right arrows | D-Pad Left / Right or left stick X | Uses reduced climb drift speed. |
| Jump off climbable | Space | A / Cross | Leaves the climbable and starts a normal jump. |

The gamepad path uses SDL's `SDL_GameController` mapping layer, so mapped controllers report a consistent button layout. The analog stick defaults to an 8000-unit dead zone, adjustable in settings. Native builds defer controller-subsystem initialization until a stable menu or game frame has presented; keyboard input remains available while it initializes.

## Start Menu and Level Select

Without `--level`, the native executable loads `levels/campaigns/main.toml`: Creator's Playground, then the two Volcanic Depths stages. The selector reads each TOML `name` (falling back to the filename). **Level Select** closes the active game screen and reopens that catalog in the same session. Separate mechanics examples use `--level levels/labs/NAME.toml`.

`--level <path>` and `--sandbox` start gameplay directly and skip the selector. `--level` has no campaign-membership check, so a valid TOML level may be launched even when it is not listed in the manifest. The current catalog is documented in the generated [Level Catalog](../level-catalog/).

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| Previous level | Left, A, or Up | D-pad Left or Up |
| Next level | Right, D, or Down | D-pad Right or Down |
| Play selected level | Enter or Space | A / Cross or Start |
| Exit menu | Esc | B / Circle or Back |

The selected level wraps at either end of the manifest-defined catalog. A held confirm carried from a prior screen must be released before it can start the selected level. A missing or invalid manifest prevents the native menu from opening; fix the manifest or its listed TOML files rather than expecting a fallback selector.

## Pause and Terminal Overlays

| State | Keyboard | Gamepad | Behaviour |
|-------|----------|---------|-----------|
| Active gameplay | Esc | Start | Toggle player pause. |
| Pause overlay | Enter, Space, or Esc | Start | Clear player pause. A focus pause still waits for focus to return; window close exits. Back opens settings; B has no pause action. |
| Terminal overlays | Up or W / Down or S | D-pad Up / Down | Move focus with wraparound. |
| Terminal overlays | Enter or Space | A / Cross or Start | Confirm focused action. |
| Terminal overlays | Esc | B / Circle or Back | Exit the run immediately; does not confirm the focused action. |

| Overlay | Actions, in focus order |
|---------|-------------------------|
| Completion with `next_phase` | Next Level, Replay, Level Select, Exit |
| Final completion | Replay, Level Select, Exit |
| Game over | Retry, Level Select, Exit |

**Next Level** loads the resolved `next_phase` in the current game session. A failed load leaves the completion overlay and its focus intact. **Retry** restarts the current level in place with level-defined hearts and lives, score reset, and music resumed. **Level Select** returns to the start menu. **Exit** ends the application session.

The overlay text is snapshotted in [Overlay Snapshots](../overlay-snapshots/) so docs drift checks catch stale copy and control hints.

## Settings and Saved Progress

- **Open:** F1 on either screen; gamepad Y in the start menu or Back during gameplay/pause. On terminal overlays, Back exits; use F1 for settings.
- **Navigate:** Up/Down or D-pad selects a row; Left/Right changes values; Enter/Space or A/Start activates. Esc or B/Back closes, or cancels binding capture first.
- **Options:** music/effects volume, mute, stick dead zone, native window scale (1×–4×), high-contrast outlines, reduced motion, control remapping and restore defaults.
- **Remapping:** Left, Right, Up, Down, Jump and Run each have keyboard and gamepad bindings. Reserved/duplicate bindings are rejected; arrows remain available. Inspector keys are reserved in debug sessions.

Settings apply when the panel closes. Normal runs save settings, the last played
stage and per-level best score/time/coin results. `--continue` opens that stage
from its start, not a mid-level checkpoint; all campaign levels remain selectable.
Native profiles use `profile.toml` under `SDL_GetPrefPath("SuperMango", "SuperMango")`,
or an explicit `--profile PATH`. Browser profiles use localStorage and Web Locks;
unsupported/denied storage or conflicting saves produce a visible profile warning
(F1 shows details). Exit/replay waits for a pending save to settle. Debug, smoke,
scripted replay, experiment replay and `--no-save` runs do not read or write the
personal profile. Editor playtests pass `--no-save`.

## Browser / WebAssembly Input

The GitHub Pages cabinet uses the WebAssembly build generated by `make web`. Its normal and debug launchers deliberately pass `--level levels/00_sandbox_01.toml`, so they boot that TOML level directly rather than showing the native campaign selector; Debug Mode also adds `--debug`. Keyboard controls match the native build. A mapped controller follows the same SDL input route when the browser exposes one, but keyboard remains the dependable browser control path.

SDL binds browser keyboard listeners to `#canvas`. Every game-screen creation dispatches synthetic key-up events for supported controls and clears queued keyboard events before input begins, preventing stale movement or confirm state from surviving a focus or screen transition. Held controls are release-latched across menu, replay, and phase transitions, so they must be released before they affect the new game screen.

Tab leaves the game canvas. Keyboard browser shortcuts remain available. The
embedded and standalone hosts provide touch buttons for Left/Right/Up/Down,
Jump/Confirm, Run, Pause/Back and Settings. Multiple pointers can hold movement
and jump together; cancellation, focus loss and screen changes clear held input.
Touch actions remain independent of remapped keyboard keys and are disabled
until the runtime starts and after it exits.

### Browser Replay

Selecting **Replay** on a completion overlay is deliberately a browser reload, not an in-place game replacement. The session stores the current TOML path under `super-mango-replay-level` in `sessionStorage`, cancels its Emscripten callback, closes the game and runtime once, then reloads the page. The Pages host consumes and removes that one-shot value and invokes the WebAssembly program with `--level <stored path>`. Replay resumes the same level in normal mode. If session storage fails, the completion overlay remains active, no reload occurs, and the runtime logs `Replay unavailable: storage failed`.

## Debug Experiments

`--debug` opens the simulation inspector and disables personal-profile persistence.
F2 freezes/resumes; F3 advances one 1/60-second step (the recorded dt during replay);
F4 cycles 1×, 0.25× and 0.1× speed. F6 selects a movement field, minus/equal adjusts
it, F7 restores authored values, and F10 cycles player/fish/platform/saw inspection.
Focus/settings/player pause and terminal screens retain priority.

F8 restarts the current level and records semantic inputs, actual simulation dt,
movement tuning and seed. F9 explicitly exports the capture to a native file or
browser download, preserving existing files. Replay requires unchanged level
bytes and the same engine revision. It ignores live movement and freezes at the
last recorded step. Captures are bounded to 36,000 simulation steps and do not
span level transitions. See [Mechanics Museum](../mechanics-museum/).

## Runtime Flags for Input and CI

| Flag | Use |
|------|-----|
| `--help` | Prints usage and exits. |
| `--debug` | Enables the inspector, FPS/frame interval, memory, hitboxes and event log; disables personal-profile persistence. |
| `--sandbox` | Loads `levels/00_sandbox_01.toml` directly. |
| `--level <path>` | Starts gameplay from a specific TOML file and skips the start menu. |
| `--continue` | Opens the last saved stage if available and no explicit level was supplied; otherwise opens the selector. |
| `--profile <path>` | Uses an explicit native profile file. |
| `--no-save` | Keeps settings/results in memory only; does not read or write the personal profile. |
| `--experiment <path>` | Replays an exported capture; requires `--level`, enables debug/no-save, and cannot combine with `--replay-script`. |
| `--smoke-test-frames N` | Runs exactly `N` frames and exits for bounded CI smoke checks. |
| `--seed N` | Seeds the explicit unsigned PRNG for reproducible enemy timers and experiments. |
| `--replay-script <name>` | Injects `out/replays-smoke/<name>.replay` as deterministic SDL key events. |

Deterministic replay scripts are generated by `tools/run_scripted_smoke.py` and consumed by `src/input/game_replay.c`. They are CI test fixtures, distinct from the player-facing Browser Replay action.

## Related Pages

- [Build System](../build-system/) — Make targets and CI smoke commands.
- [Player Module](../player-module/) — movement, physics, animation, hitbox, and climb logic.
- [Level Design](../level-design/) — TOML schema and per-level physics overrides.
