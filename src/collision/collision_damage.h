/*
 * collision_damage.h — Damage application system public interface.
 *
 * Centralized damage and knockback handler used by collision detection
 * and other game systems (e.g., sea gap instant kill).
 */
#pragma once

#include "../game.h"

/* Forward declaration — implemented in game_state.c */
void reset_current_level(GameState *gs, int *fp_prev_riding);

/* ------------------------------------------------------------------ */
/* Damage application                                                 */
/* ------------------------------------------------------------------ */

/*
 * apply_damage — Centralized damage and knockback handler.
 *
 * amount   : hearts to remove. Pass gs->hearts for an instant-kill
 *            (e.g., sea gap) that bypasses the normal 1-heart decrement.
 *
 * push     : 1 = apply a knockback impulse opposite to the contact
 *            direction; 0 = skip (sea gap fall has no contact surface).
 *
 * src_cx / src_cy : world-space centre of the damage source.
 *            Used only when push=1 and the player is stationary — the
 *            impulse pushes the player away from this point.
 *
 * On lethal damage (hearts <= 0 after applying amount), this function
 * decrements lives and either resets the level (if lives remain) or
 * triggers game over.
 */
void apply_damage(GameState *gs, int amount, int push,
                  float src_cx, float src_cy);
