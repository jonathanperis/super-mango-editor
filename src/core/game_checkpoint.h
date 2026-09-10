/*
 * game_checkpoint.h — Authored and legacy checkpoint updates.
 */

#pragma once

#include "../game.h"

/* Sample authored placements before lethal collision damage can reset player. */
void game_checkpoint_update_authored(GameState *gs);

void game_checkpoint_update(GameState *gs);

void game_checkpoint_feedback_set(GameState *gs, CheckpointFeedbackKind kind,
                                  Uint32 now, Uint32 duration);
void game_checkpoint_feedback_clear_expired(GameState *gs, Uint32 now);
