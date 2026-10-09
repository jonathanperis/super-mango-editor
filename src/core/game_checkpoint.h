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

void game_checkpoint_feedback_set(GameState *gs, CheckpointFeedbackKind kind,
                                  uint32_t now, uint32_t duration);
void game_checkpoint_feedback_clear_expired(GameState *gs, uint32_t now);
