/*
 * editor_minimap.h — The whole level at a glance, along the status bar.
 *
 * A thin strip in the free bottom rows of the status bar, under the canvas,
 * shows the full width of the level: a flat-colour rectangle per entity
 * (coloured by palette category), the floor with its gaps, and a frame
 * around the part the canvas shows. A click or drag on it scrolls the
 * canvas there. It covers nothing of the level or the panels.
 */

#ifndef EDITOR_MINIMAP_H
#define EDITOR_MINIMAP_H

#include "editor.h" /* EditorState, CANVAS_W, EDITOR_H */

/* The strip: under the canvas, in the status bar rows below its text. */
#define EDITOR_MINIMAP_X 4
#define EDITOR_MINIMAP_WIDTH (CANVAS_W - 8)
#define EDITOR_MINIMAP_HEIGHT 7
#define EDITOR_MINIMAP_Y (EDITOR_H - EDITOR_MINIMAP_HEIGHT - 1)

/* Whether screen point (sx, sy) is on the strip. */
int editor_minimap_contains(int sx, int sy);

/* The world x a strip column stands for (clamped to the level). */
float editor_minimap_world_x(const EditorState *es, int sx);

/* Centre the canvas on the world x under strip column sx (zoom kept). */
void editor_minimap_jump(EditorState *es, int sx);

/* Where a world rectangle lands on the strip, clipped to it: 1 and the
 * screen rectangle when any of it shows, 0 otherwise. */
int editor_minimap_strip_rect(float world_w, float x, float y, float w, float h,
                              int *left, int *top, int *width, int *height);

/* Draw the strip, and scroll the canvas while a press that began on it is
 * held. Called by the status bar; nothing while the Campaign view is open. */
void editor_minimap_render(EditorState *es);

#endif /* EDITOR_MINIMAP_H */
