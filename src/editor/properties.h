/*
 * properties.h — Public interface for the entity property inspector panel.
 *
 * When an entity is selected on the canvas, the properties panel appears in
 * the bottom half of the right column (below the palette).  It shows editable
 * fields specific to the selected entity type — position, patrol range,
 * animation frame, mode dropdown, etc.
 *
 * The panel uses the immediate-mode UI widgets from ui.h (ui_float_field,
 * ui_int_field, ui_dropdown, ui_label, ui_panel, ui_separator) so every
 * field is re-rendered each frame and edits flow directly into the LevelDef
 * stored in EditorState.
 *
 * Include this header in editor.c (or wherever the right-panel rendering is
 * orchestrated) to call properties_render each frame.
 */
#pragma once

/*
 * Include editor.h for the full EditorState definition.
 *
 * EditorState is defined as an anonymous typedef struct in editor.h and
 * cannot be forward-declared.  Including the header is safe because
 * editor.h uses #pragma once and does not include properties.h (no cycle).
 */
#include "editor.h"

/*
 * properties_render — Draw the property inspector for the selected entity.
 *
 * Called once per frame from the editor's main render path.  If no entity
 * is selected (es->selection.index < 0), the function returns immediately
 * and draws nothing.  Otherwise it renders a panel background, a header
 * showing the entity type name and array index, and per-type editable
 * fields (floats, ints, dropdowns) that write directly into the LevelDef.
 *
 * When any field value changes, es->modified is set to 1 so the title bar
 * shows the unsaved-changes indicator.
 *
 * start_y     : top edge of the section in window pixels.
 * available_h : total height available for this section.
 *
 * es : pointer to the editor state (reads selection, level, ui; writes
 *      level field values and modified flag).
 */
void properties_render(EditorState *es, int start_y, int available_h);

/*
 * level_config_render — Draw the level-wide configuration panel.
 *
 * Shows editable fields for parallax layers, player spawn, music, and fog.
 * Rendered as the topmost section of the right panel so level-wide settings
 * are always accessible regardless of entity selection.
 *
 * start_y     : top edge of the section in window pixels.
 * available_h : total height available for this section.
 */
void level_config_render(EditorState *es, int start_y, int available_h);
