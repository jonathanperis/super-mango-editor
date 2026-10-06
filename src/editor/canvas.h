/*
 * canvas.h — Public interface for the editor canvas (world viewport).
 *
 * The canvas renders the game world inside the left portion of the
 * editor window (CANVAS_W x CANVAS_H pixels).  World width is dynamic,
 * calculated from the level's screen_count (screen_count * GAME_W).
 * Entities are drawn at their exact game display sizes and derived Y
 * positions so the editor is WYSIWYG with the running game.
 *
 * Functions:
 *   canvas_render          — draw the full level preview for the current frame.
 *   canvas_screen_to_world — convert a screen pixel to world-space coordinates.
 *   canvas_contains        — test whether a screen pixel falls inside the canvas.
 */
#pragma once

/*
 * Include editor.h for the full EditorState definition.
 *
 * EditorState is defined as an anonymous struct (typedef struct { ... })
 * in editor.h, which cannot be forward-declared in C.  Including the
 * full header is safe because editor.h uses #pragma once and there is
 * no circular dependency (editor.h does not include canvas.h).
 */
#include "editor.h"

/*
 * canvas_render — Draw the entire level canvas for one frame.
 *
 * Draws sky, floor (9-slice), platforms, water, every entity type at its
 * game-accurate size and position, selection highlight, ghost preview,
 * and optional grid overlay.  Uses EditorState->camera for scroll / zoom.
 */
void canvas_render(EditorState *es);

/*
 * canvas_screen_to_world — Convert screen coordinates to world-space.
 *
 * Reverses the camera zoom and scroll transform so mouse clicks on the
 * canvas can be mapped to level positions.
 *
 * sx, sy : screen-space pixel coordinates (window pixels).
 * wx, wy : output world-space coordinates (logical pixels).
 */
void canvas_screen_to_world(const EditorState *es, int sx, int sy,
                            float *wx, float *wy);

/*
 * canvas_clamp_camera — Keep camera.x / camera.y inside the world.
 *
 * The view may not scroll past the right or bottom edge; when the whole
 * world fits (e.g. its 300-px height at 1x/2x) the offset is 0.  Called
 * every frame, so zoom, screen_count or level changes can never leave the
 * camera looking at empty space.
 */
void canvas_clamp_camera(EditorState *es);

/*
 * canvas_set_zoom — Change zoom while keeping the world point under the
 * screen pixel (anchor_sx, anchor_sy) fixed, then clamp the camera.
 * Ctrl+wheel anchors at the cursor; the toolbar uses the canvas centre.
 */
void canvas_set_zoom(EditorState *es, float zoom, int anchor_sx, int anchor_sy);

/*
 * canvas_contains — Test whether a screen point is inside the canvas area.
 *
 * Returns 1 if (sx, sy) falls within the canvas rectangle
 * (0..CANVAS_W, TOOLBAR_H..TOOLBAR_H+CANVAS_H), 0 otherwise.
 */
int canvas_contains(int sx, int sy);
