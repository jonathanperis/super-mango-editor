# Mechanics Museum

These small, standalone levels each isolate a mechanism. They are separate from
the three-stage campaign and have no progression chain. All commands run from
the repository root after `make builder`.

| Level | Start command | Observe |
|-------|---------------|---------|
| One-way collision | `make run-level-debug LEVEL=levels/labs/01_collision.toml` | Jump-through and landing; the foot point used by the crossing test |
| Moving support | `make run-level-debug LEVEL=levels/labs/02_moving_platforms.toml` | Rail motion and rider displacement |
| Checkpoints | `make run-level-debug LEVEL=levels/labs/03_checkpoints.toml` | Cross a checkpoint, fall in the gap, respawn |
| Climbing | `make run-level-debug LEVEL=levels/labs/04_climbing.toml` | Ladder, rope and vine state changes |
| Hazard timing | `make run-level-debug LEVEL=levels/labs/05_hazards.toml` | Current-frame saw collision, spikes and flame states |
| Motion and camera | `make run-level-debug LEVEL=levels/labs/06_camera.toml` | Acceleration, friction, lookahead and parallax |

## Inspector

Every command above starts the game with `--debug`, which turns on the
inspector: hitboxes, the player's foot contact, velocity and state, and the
keys to freeze (F2), step one 1/60 s tick (F3) and slow down (F4) the
simulation. F5 lists every key in the game, and
[Controls](../controls/#debug-inspector-keys) has the full table, including
live tuning (F6, `-`/`=`, F7) and recording (F8, F9).

A good habit in each level: freeze just before the moment you care about,
then step through it one tick at a time and watch the numbers change.

Run `make timing-lab` for the lesson behind the fixed step: it integrates a
one-second free fall at 30, 60 and 144 Hz render rates and shows that a variable
`dt` lands at a different height on each, while fixed 1/60 s steps give the same
result (with the same small integration error) everywhere.

## Record and replay

F8 restarts the level and records every step; F9 saves the recording as a
TOML file. Export it before you leave the level: choosing Replay, Level
Select, Exit or the next level throws the recording away. Play it back with
`--level <same level> --experiment <file>`; the game refuses if the level
file has changed since, because the same inputs would no longer give the same
run. [Controls](../controls/#recording-and-replaying-an-experiment) explains
the file format and its limits, and [Sandbox School](../learning-path/) lab 8
walks through it.

## Edit safely

Open a lab in the editor and Save As a new level before experimenting. Validation
keeps malformed drafts editable but blocks saving/playtesting invalid data.
`make validate-levels` checks campaign and museum content; `make smoke` boots both.
The multi-frame regression scenarios use separate `tests/fixtures/runtime/`
data, so rearranging a learning example does not change their expected geometry.
