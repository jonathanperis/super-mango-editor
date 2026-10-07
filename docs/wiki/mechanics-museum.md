# Mechanics Museum

These small, standalone levels each isolate a mechanism. They are separate from
the three-stage campaign and have no progression chain. All commands run from
the repository root after `make builder`.

| Level | Start command | Observe |
|-------|---------------|---------|
| One-way collision | `make run-level-debug LEVEL=levels/labs/01_collision.toml` | Jump-through and landing; previous/current foot positions |
| Moving support | `make run-level-debug LEVEL=levels/labs/02_moving_platforms.toml` | Rail motion and rider displacement |
| Checkpoints | `make run-level-debug LEVEL=levels/labs/03_checkpoints.toml` | Cross a checkpoint, fall in the gap, respawn |
| Climbing | `make run-level-debug LEVEL=levels/labs/04_climbing.toml` | Ladder, rope and vine state changes |
| Hazard timing | `make run-level-debug LEVEL=levels/labs/05_hazards.toml` | Current-frame saw collision, spikes and flame states |
| Motion and camera | `make run-level-debug LEVEL=levels/labs/06_camera.toml` | Acceleration, friction, lookahead and parallax |

## Inspector

The game simulates in fixed 1/60 s steps (`src/core/game_timing.c` keeps an
accumulator of real time), so the inspector controls how many steps run, never
their length. Use F2 to freeze, F3 to freeze and advance exactly one step, and F4
to cycle normal, 1/4 and 1/10 speed (fewer steps per second, each still 1/60 s).
F6 cycles the nine movement properties; minus/equal changes the selected value by
25 in its documented units. F7 restores the level's movement settings. Values are
bounded between zero and `MAX_LEVEL_MOTION`. Player position, velocity,
animation/climbing state, hitboxes, foot contact, moving-support index and
checkpoint state remain visible in debug mode. F10 cycles a detail row through
the player, fish, floating platforms and saws present in the level. During
capture replay, F4, F7 and minus/equal are ignored, so live speed and tuning
changes cannot alter the run; F2 and F3 still pause and step it. Inspector
function keys and minus/equal are reserved during debug remapping; ordinary
non-debug bindings remain compatible.

Run `make timing-lab` for the lesson behind the fixed step: it integrates a
one-second free fall at 30, 60 and 144 Hz render rates and shows that a variable
`dt` lands at a different height on each, while fixed 1/60 s steps give the same
result (with the same small integration error) everywhere.

F8 restarts the level with its seed and records at most 36,000 active simulation
steps (10 minutes). F9 exports the capture; it never overwrites an existing destination.
Recording is scoped to one level. Export before choosing a terminal route;
switching levels or replaying the game disposes of that screen's capture.
The capture (`format_version = 2`) stores one row per fixed 1/60 s step,
`[input bits, nine movement values]` with no duration column, plus the level
path, the seed and a content fingerprint of the source level. Replay it with
`--experiment <file>`; the game freezes after the last recorded step. A changed level is rejected rather than
silently producing a different run, and so is a `format_version = 1` capture:
it was recorded by the earlier variable-timestep engine and cannot be replayed
faithfully, so record it again. If an external editor
changes the loaded file, reopen the game before starting a new capture. Captures are not save
games and do not include pause duration, audio output or rendered pixels.

Focus loss, settings, intentional player pause and terminal overlays still block
simulation. A queued step is discarded while blocked; it does not fire later
when you return. Debug sessions do not read or write personal profiles.

## Edit safely

Open a lab in the editor and Save As a new level before experimenting. Validation
keeps malformed drafts editable but blocks saving/playtesting invalid data.
`make validate-levels` checks campaign and museum content; `make smoke` boots both.
The multi-frame regression scenarios use separate `tests/fixtures/runtime/`
data, so rearranging a learning example does not change their expected geometry.
