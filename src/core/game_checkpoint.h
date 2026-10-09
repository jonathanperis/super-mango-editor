/*
 * game_checkpoint.h — Authored and legacy checkpoint updates.
 */

#pragma once

#include "../game.h"

/* Sample authored placements before lethal collision damage can reset player. */
void game_checkpoint_update_authored(GameState *gs);

/* Legacy levels (no authored placements): save at each newly entered screen.
 * Does nothing for levels with authored placements; those are sampled once
 * per step by game_checkpoint_update_authored. */
void game_checkpoint_update(GameState *gs);

/*
 * Checkpoint feedback — the "CHECKPOINT" / "RESPAWN" banner on the HUD.
 *
 * Its deadline is measured on game_checkpoint_clock_ms: simulated time, not
 * the wall clock. Only fixed simulation steps move that clock, so a banner
 * waits out a pause instead of expiring behind the pause menu, and a replay
 * shows it for exactly the same steps every time.
 */

/* Simulated milliseconds: gs->world.sim_steps fixed steps of 1000/60 ms,
 * rounded down, wrapping like a 32-bit ms clock. It restarts at 0 whenever
 * a level is applied (load, next phase, F8 restart). */
uint32_t game_checkpoint_clock_ms(const GameState *gs);

void game_checkpoint_feedback_set(GameState *gs, CheckpointFeedbackKind kind,
                                  uint32_t now, uint32_t duration);
void game_checkpoint_feedback_clear_expired(GameState *gs, uint32_t now);

/* Count one fixed step (when dt > 0), then hide the banner if its time is
 * up. game_update_active calls this once per step. */
void game_checkpoint_feedback_tick(GameState *gs, float dt);
