# Controls & Input

<a id="home"></a>

---

Super Mango accepts keyboard input everywhere, SDL gamepad input when a mapped controller is available, deterministic replay input for CI, and browser-safe keyboard input for the WebAssembly cabinet on GitHub Pages.

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

The gamepad path uses SDL's `SDL_GameController` mapping layer, so Xbox, DualShock, DualSense, and other mapped controllers report a consistent button layout. The analog stick uses an 8000-unit dead zone to avoid idle drift. Native builds defer controller-subsystem initialization until a stable menu or game frame has presented; keyboard input remains available while it initializes.

## Start Menu and Level Select

Without `--level`, the native executable loads `levels/campaigns/main.toml`: Creator's Playground, then the two Volcanic Depths stages. The selector reads each TOML `name` (falling back to the filename). **Level Select** closes the active game screen and reopens that catalog in the same session. Separate mechanics examples use `--level levels/labs/NAME.toml`.

## Debug experiments

`--debug` opens the simulation inspector and disables personal-profile persistence.
F2 freezes/resumes simulation; F3 advances one 1/60-second step; F4 cycles 1×,
0.25× and 0.1× speed. F6 selects a movement field, minus/equal adjusts it, and F7
restores authored values. Focus/settings/player pause and terminal screens retain
priority over these controls.

F8 restarts the current level and records semantic inputs, actual simulation dt,
movement tuning and seed. F9 explicitly exports the capture (native working
directory or browser download), preserving existing files. Use
`--level PATH --experiment CAPTURE.toml` to replay with unchanged level bytes and
the same engine revision. Live movement is ignored during replay, which freezes
at the last recorded step. Captures are bounded to 36,000 simulation steps and
do not span level transitions. See [Mechanics Museum](../mechanics-museum/).

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
| Pause overlay | Enter, Space, or Esc | Start | Clear player pause. A focus pause still waits for focus to return; window close exits; B/Back has no pause action. |
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

## Browser / WebAssembly Input

The GitHub Pages cabinet uses the WebAssembly build generated by `make web`. Its normal and debug launchers deliberately pass `--level levels/00_sandbox_01.toml`, so they boot that TOML level directly rather than showing the native campaign selector; Debug Mode also adds `--debug`. Keyboard controls match the native build. A mapped controller follows the same SDL input route when the browser exposes one, but keyboard remains the dependable browser control path.

SDL binds browser keyboard listeners to `#canvas`. Every game-screen creation dispatches synthetic key-up events for supported controls and clears queued keyboard events before input begins, preventing stale movement or confirm state from surviving a focus or screen transition. Held controls are release-latched across menu, replay, and phase transitions, so they must be released before they affect the new game screen.

### Browser Replay

Selecting **Replay** on a completion overlay is deliberately a browser reload, not an in-place game replacement. The session stores the current TOML path under `super-mango-replay-level` in `sessionStorage`, cancels its Emscripten callback, closes the game and runtime once, then reloads the page. The Pages host consumes and removes that one-shot value and invokes the WebAssembly program with `--level <stored path>`. Replay resumes the same level in normal mode. If session storage fails, the completion overlay remains active, no reload occurs, and the runtime logs `Replay unavailable: storage failed`.

## Runtime Flags for Input and CI

| Flag | Use |
|------|-----|
| `--debug` | Enables FPS, CPU frame time, memory, hitboxes, and event log overlay. |
| `--sandbox` | Loads `levels/00_sandbox_01.toml` directly. |
| `--level <path>` | Starts gameplay from a specific TOML file and skips the start menu. |
| `--smoke-test-frames N` | Runs exactly `N` frames and exits for bounded CI smoke checks. |
| `--seed N` | Seeds `rand()` for deterministic smoke and replay behaviour. |
| `--replay-script <name>` | Injects `out/replays-smoke/<name>.replay` as deterministic SDL key events. |

Deterministic replay scripts are generated by `tools/run_scripted_smoke.py` and consumed by `src/input/game_replay.c`. They are CI test fixtures, distinct from the player-facing Browser Replay action.

## Related Pages

- [Build System](../build-system/) — Make targets and CI smoke commands.
- [Player Module](../player-module/) — movement, physics, animation, hitbox, and climb logic.
- [Level Design](../level-design/) — TOML schema and per-level physics overrides.
