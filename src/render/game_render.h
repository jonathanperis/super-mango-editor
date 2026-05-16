/*
 * game_render.h — Rendering system public interface.
 *
 * Handles all game rendering including the 32 render layers,
 * parallax backgrounds, entities, player, effects, HUD, and overlays.
 */
#pragma once

#include "../game.h"

/* ------------------------------------------------------------------ */
/* Main render function                                               */
/* ------------------------------------------------------------------ */

/*
 * game_render_frame — Draw every layer for one frame.
 *
 * Called from game_loop_frame after the update phase, and also via the
 * 'render:' goto on the paused path so the last frame stays visible.
 * cam_x is the integer camera offset (world → screen conversion).
 */
void game_render_frame(GameState *gs, int cam_x, float dt);

/* ------------------------------------------------------------------ */
/* Overlay rendering                                                  */
/* ------------------------------------------------------------------ */

/*
 * render_pause_overlay — Draw the gameplay pause screen.
 *
 * Shows a semi-transparent overlay on top of the last rendered gameplay frame
 * with resume controls.
 */
void render_pause_overlay(GameState *gs);

/*
 * render_level_complete_overlay — Draw the level complete screen.
 *
 * Shows semi-transparent black overlay with "Level Complete!" title,
 * final score, and exit hint. Called from game_render_frame when
 * gs->completion.complete is true.
 */
void render_level_complete_overlay(GameState *gs);
