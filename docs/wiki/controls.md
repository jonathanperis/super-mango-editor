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
| Continue the selected level from its saved point (when shown) | C | X / Square |
| Exit menu | Esc | B / Circle or Back |

The selector lists every campaign level (up to five rows at a time, scrolling with the selection). Each row shows a gold `*` when the profile records a finished run of that level (`-` otherwise), the level name, the best time as `m:ss.cc` and the best coins out of the level's total, such as `3/12`; an uncleared level shows `--`. The line under **Play** repeats the selected level's best time, coins and score, or says it is not cleared yet.

When the profile holds a Continue point for the selected level (see [Continue](#continue)), a **Continue** button appears to the left of **Play**, the line under them says where it picks up (`Continue from checkpoint 2: 120 pts, 2 lives`), and the hint line adds `C/X: continue`. **Play** still starts the level from its beginning.

The mouse can also click a list row to select that level, **Continue**, **Play** and the **Settings (F1 / Y)** button. The selected level wraps at either end of the manifest-defined catalog. A held confirm carried from a prior screen must be released before it can start the selected level. A missing or malformed manifest, or one with no playable level, prevents the native menu from opening. A single listed level that fails to load stays in the selector, greyed out with an "Unavailable" reason, and cannot be started; the rest remain playable (see [Campaign Manifest](../level-design/#campaign-manifest-v1)).

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
released, so the Space or A that resumed a pause does not also jump. A new game
screen (Play, Replay, a direct `--level` start) arms it once, after the profile's
bindings are attached, including controls still held from the previous screen.

The overlay text is snapshotted in [Overlay Snapshots](../overlay-snapshots/) so docs drift checks catch stale copy and control hints.

## Settings and Saved Progress

- **Open:** F1 on either screen; gamepad Y in the start menu or Back during gameplay/pause. On terminal overlays, Back exits; use F1 for settings.
- **Navigate:** Up/Down or D-pad selects a row, wrapping at the ends; Left/Right changes values on the main page; Enter, keypad Enter, Space or A/Start activates, and a mouse click activates a row. Esc or B/Back closes, or cancels binding capture first; either one also clears the message about a refused binding. The start menu's **Settings** button opens the panel exactly as F1 does.
- **Options:** music/effects volume (0–128 in steps of 8), mute, stick dead zone (0–28000 in steps of 1000), native window scale (1×–4×; not applied in the browser), high-contrast outlines, reduced motion, **Ghost (best run)** on/off (default on; see [Time Trial Ghost](#time-trial-ghost)), control remapping (a separate page of 12 binding rows) and **Restore default settings**, which resets every setting, not only controls.
- **Remapping:** Left, Right, Up, Down, Jump and Run each have keyboard and gamepad bindings. Duplicates are rejected, as are reserved keys (Esc, F1, Tab, Enter, keypad Enter, the arrows and Right Shift) and buttons (Back, Start, Guide, B); misc, paddle and touchpad buttons cannot be captured. Arrows remain available. F2–F10, `-` and `=` belong to the [debug inspector](#debug-inspector-keys) and are refused in every run, because normal and debug runs share one profile. A profile saved with one of them bound has that control put back on its default key (every key, if that default is taken), with a warning; the rest of the profile still loads.

Settings apply when the panel closes. Normal runs save settings, the last played
stage, per-level best score/time/coin results and time-trial ghosts. A profile holds results for up
to 128 levels; a coin result is at most `MAX_COINS` (64), the most coins one level
can place, so raising that constant keeps old profiles readable. A result that
cannot be recorded (a full profile) is logged as a warning rather than dropped
silently. The profile also keeps one Continue point (below); all campaign levels
remain selectable.

### Continue

A normal run records a **Continue point** for the level being played whenever the
player reaches a new respawn point (an authored checkpoint, or the next screen in
a level without authored checkpoints), whenever a pause begins (the browser pauses
when its tab is hidden, the only warning before a tab closes), and when the player
leaves the level part-way with Exit or Level Select. It holds the respawn point,
score, lives, the coins already collected and the level timer, plus the level
file's content hash. Finishing the level or losing its last life clears it; the
profile keeps only one, for the most recent level. A run started with
`--start-x` or `--start-checkpoint` never records, replaces or clears it (see the
[runtime flags](#runtime-flags-for-input-and-ci)).

**Continue** in the start menu, or `--continue` on the command line, reopens that
level and puts the player on the saved respawn point exactly as a lost life would:
enemies and hazards restart from their authored places, hearts are full, collected
coins stay collected, and the HUD shows `RESPAWN`. Its time counts on from the
saved timer. A Continue point whose level file has changed since (a different
content hash) is dropped with a warning, before the menu offers it and again when
it is applied, and that level starts from its beginning. **Play**, **Retry**,
**Replay** and **Next Level** always start a level from its beginning. The profile
stores it as a `[resume]` table, added in profile `format_version` 2 together with
the `ghost` setting; version-1
profiles load unchanged (Ghost on, no Continue point) and are written as version 2
on the next save.

### Time Trial Ghost

Every normal run records where Mango is after each fixed 1/60 s step (his pixel
position and sprite frame). When a run finishes a level faster than that level's
stored ghost, or the level has no ghost yet, the run becomes the new ghost. Every
later attempt at that level (Play, Replay, Retry, Next Level into it) races it: a
translucent Mango drawn just behind the player shows where the best run was at the
same moment, and disappears once that run reached its star. Positions are stored
rather than inputs, so a ghost never depends on enemies or random numbers behaving
the same way twice.

- **Ghost (best run)** in Settings hides or shows it (stored as `ghost` in the
  profile). High-contrast outlines make the ghost more opaque with a cyan outline;
  reduced motion holds one pose per animation instead of cycling frames.
- A ghost is bound to its level file's content hash: after the level is edited
  the old ghost is ignored, and the next finished run replaces it.
- Runs continued from a Continue point or started with `--start-x` /
  `--start-checkpoint`, and runs longer than five minutes (`GHOST_MAX_STEPS`),
  are not whole runs and never become ghosts. Debug, smoke,
  replay, experiment and `--no-save` runs have no ghost at all.
- Native ghosts are TOML files next to the profile, named after it and the
  level: `profile-ghost-01_lugio_01.toml` beside `profile.toml` (or
  `<name>-ghost-<level>.toml` beside an explicit `--profile <name>.toml`). They
  are written through a temporary file like the profile. Browser ghosts use one
  `localStorage` entry per level, `super-mango-ghost-v1:<level path>`; when storage
  is full the ghost is simply not saved and a warning is logged. A ghost text of
  five minutes is about 180 KB; anything over 256 KB, or damaged in any way, is
  ignored.

Native profiles use `profile.toml` under the OS preference root plus `SuperMango/SuperMango/`,
or an explicit `--profile PATH`. A native save writes a sibling temporary file and
renames it over `profile.toml`. If Windows moves the old file aside but cannot put
the new one in place, the temporary file is kept (never deleted), saving stops for
that run, and F1 shows `Profile save incomplete; your profile is safe in <path>`;
renaming that file to `profile.toml` restores it. Browser profiles use localStorage and Web Locks;
unsupported/denied storage or conflicting saves produce a visible profile warning
(F1 shows details). Exit waits for a pending save to settle. Debug, smoke,
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

Selecting **Replay** on a completion overlay replaces the game in place, exactly as the native build does: the session closes the current `GameState` and opens the same TOML path in a new one inside the running page. The window, WebGL context, audio device, Emscripten frame callback and profile all stay alive, so sound keeps playing without a new click and nothing in the profile is lost. Earlier builds reloaded the whole page through a `sessionStorage` boot intent; that came from the SDL2 build, where every game screen created its own window and renderer. Since the raylib port the session owns those for the whole run, as Level Select and Play already relied on, so neither host page reads a boot intent any more.

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
run cannot change; F2 and F3 still pause and step it. These keys can never be
remapped controls, in debug or normal runs. Focus loss, the
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
as `mango-experiment-<seconds>.toml` in the working directory (or as a
browser download), where `<seconds>` is the calendar time (seconds since
1970). It never overwrites an existing file: a taken name becomes
`mango-experiment-<seconds>-2.toml`, `-3` and so on up to `-10`. The debug log
says which of "Nothing recorded; F8 starts a recording", "Export failed: file
names already taken" or "Export failed: could not write the file" stopped an
export. The file
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
| `--start-x <px>` | With `--level`: starts the first attempt with the player centred on world x `<px>` (a whole number, 0 to the level width), standing on the highest surface under it (ground floor, pillar, bridge, or a fixed or crumbling float platform). A lost life respawns there until a later checkpoint is crossed; checkpoints already behind it count as reached. Over a floor gap with nothing above it, or outside the level, the game exits with an error. The editor's **Playtest from here** passes it. |
| `--start-checkpoint <n>` | With `--level`: starts the first attempt on authored checkpoint `<n>` (0-based `[[checkpoints]]` order), exactly where its respawn puts the player. A checkpoint the level does not have is an error. Only one of `--start-x` / `--start-checkpoint` may be given, neither combines with `--replay-script` or `--experiment`, and both apply to the first game only: Retry after Game Over, Replay and the next phase start at the level's own start. That first attempt skipped part of the level, so it is a playtest even when the profile is saved: it records no best result and no ghost, and it neither saves nor clears a Continue point; the profile's settings still apply. |
| `--continue` | Opens the last saved stage if available and no explicit level was supplied, at its saved [Continue](#continue) point when it has one; otherwise, or when that stage no longer loads, opens the selector. Runs that do not read the profile (`--no-save`, `--debug`, smoke, replay scripts) always open the selector. |
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

Deterministic replay scripts are generated by `tools/run_scripted_smoke.py` and consumed by `src/input/game_replay.c`. They are CI test fixtures, distinct from the player-facing Replay action.

## Related Pages

- [Build System](../build-system/) — Make targets and CI smoke commands.
- [Player Module](../player-module/) — movement, physics, animation, hitbox, and climb logic.
- [Level Design](../level-design/) — TOML schema and per-level physics overrides.
