/*
 * palette.h — Public interface for the entity palette panel.
 *
 * The palette is the right-side panel that lists all 25 placeable entity types
 * grouped into 6 categories (World, Collectibles, Enemies, Hazards, Surfaces,
 * Decorations).  Clicking an entry selects it as the active entity type for
 * placement on the canvas.
 *
 * The panel occupies the top portion of the right column.  When no entity is
 * selected on the canvas, the palette fills the full column height.  When a
 * selection exists, the palette shrinks to the top half so the properties
 * inspector can use the bottom half.
 *
 * Include this header in editor.c (or wherever the right-panel rendering is
 * orchestrated) to call palette_render each frame.
 */
#pragma once

/*
 * Include editor.h for the full EditorState definition.
 *
 * EditorState is defined as an anonymous typedef struct in editor.h and
 * cannot be forward-declared.  Including the header is safe because
 * editor.h uses #pragma once and does not include palette.h (no cycle).
 */
#include "editor.h"

/*
 * palette_render — Draw the categorised entity palette and handle clicks.
 *
 * Called once per frame from the editor's main render path.  Draws the panel
 * background, category headers, entity name rows, and selection highlight.
 * Detects mouse clicks on rows to update es->palette_type and es->tool.
 * Handles mouse-wheel scrolling when the content exceeds the visible height.
 *
 * start_y     : top edge of the section in window pixels.
 * available_h : total height available for this section.
 *
 * es : pointer to the editor state (reads ui, selection; writes palette_type,
 *      tool on click).
 */
void palette_render(EditorState *es, int start_y, int available_h);

/* Scroll the palette content by delta pixels (positive = down). */
void palette_scroll(int delta);
