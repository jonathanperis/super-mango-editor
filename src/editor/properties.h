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
 * When any field commits, one undoable property command is recorded and the
 * save-point dirty marker is refreshed.
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
 * start_y        : top edge of the section in window pixels.
 * available_h    : visible height allocated for this section (capped by caller).
 * total_content_h: uncapped full height of all content; used to clamp scroll.
 */
void level_config_render(EditorState *es, int start_y, int available_h,
                         int total_content_h);

/*
 * properties_message_list_contains — 1 when (x, y) is over the validation
 * rows as last drawn and there are more rows than fit, so the wheel there
 * scrolls the list instead of the panel.
 */
int properties_message_list_contains(int x, int y);

/*
 * properties_focus_config — Open the Level Config panel at the field a
 * validation location names ("screen_count", "physics" + "air_friction",
 * "fog_layers"...): unfold its group and give a text field the caret.
 * Returns 1 when the key is a Level Config key, 0 otherwise.
 */
int properties_focus_config(EditorState *es, const LevelIssueLocation *where);

/*
 * properties_focus_entity_field — When the properties panel next draws
 * entity (type, index), focus its `field` (a TOML key such as "vx" or
 * "tile_count"): a text field takes the caret, a dropdown opens.  An
 * empty field, or a key the entity has no field for, focuses nothing.
 */
void properties_focus_entity_field(EditorState *es, EntityType type, int index,
                                   const char *field);

/*
 * A shared field (one row of a several-entity panel) left some other way
 * than Enter or Tab: a click elsewhere, Ctrl+S, F5, Undo.
 * editor_finish_field_edit asks properties_shared_field_active before it
 * stores the typed value, which says whether the active field is the
 * shared row the panel last drew, with the same selection still there.
 * After storing a changed value it calls properties_apply_shared_field,
 * which copies it to every selected entity as one undo step, exactly as
 * Enter would (properties_apply_to_selection).
 */
int properties_shared_field_active(const EditorState *es);
void properties_apply_shared_field(EditorState *es);

/*
 * Where a layer row's three small buttons sit, in pixels from the panel's
 * left edge: move up, move down, remove (config_layer_row in
 * properties_config.c).  They fill the space between the speed field and
 * the panel's scrollbar.  Tests click them by these offsets.
 */
#define CONFIG_LAYER_BUTTON_W  16
#define CONFIG_LAYER_UP_X      328
#define CONFIG_LAYER_DOWN_X    345
#define CONFIG_LAYER_REMOVE_X  362

/* Level Config foldout state shared with layout measurement. */
extern int g_plx_open;
extern int g_fg_open;
extern int g_fog_open;
extern int g_phys_open;
