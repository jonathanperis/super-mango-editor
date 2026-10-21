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
#include "hit_test.h"  /* EditorRect */

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
 * The zoom levels the toolbar list, Ctrl+wheel and Fit choose from, from
 * a quarter (a whole long level in view) to 5x.  canvas_zoom_index is the
 * preset equal to zoom, or -1.
 */
#define CANVAS_ZOOM_COUNT 6
float canvas_zoom_preset(int index);
const char *canvas_zoom_label(int index);
int canvas_zoom_index(float zoom);

/* Zoom to the largest preset that shows the whole level (its width and
 * height), or for a level too long for every preset the exact zoom that
 * does, and scroll to its start. */
void canvas_zoom_to_fit(EditorState *es);

/*
 * canvas_contains — Test whether a screen point is inside the canvas area.
 *
 * Returns 1 if (sx, sy) falls within the canvas rectangle
 * (0..CANVAS_W, TOOLBAR_H..TOOLBAR_H+CANVAS_H), 0 otherwise.
 */
int canvas_contains(int sx, int sy);

/* The red "cannot load this level yet" banner along the canvas's bottom
 * edge, shown while the level is drawable but fails the game's checks:
 * is (sx, sy) on it?  A click there goes to the problem. */
#define CANVAS_BANNER_H 20
#define CANVAS_BANNER_Y (TOOLBAR_H + CANVAS_H - CANVAS_BANNER_H)
int canvas_banner_contains(EditorState *es, int sx, int sy);

/* Whether any part of world rectangle r is inside the canvas's view. */
int canvas_shows(const EditorState *es, const EditorRect *r);

/* Scroll so world rectangle r sits in the middle of the canvas (as far as
 * the world's edges allow), keeping the zoom. */
void canvas_center_on(EditorState *es, const EditorRect *r);
