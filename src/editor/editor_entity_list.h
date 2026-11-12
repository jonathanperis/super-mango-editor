/*
 * editor_entity_list.h — The Entities list: every placed entity, by type,
 * with a search field.
 *
 * Ctrl+F (or the toolbar's Entities button) opens the list in the right
 * column, where it takes the place of Level Config and the palette until
 * it is closed; the selection's properties stay underneath it.  Each row
 * reads "Coin #3 (120, 96)".  Clicking a row selects that entity and
 * centres the canvas on it; Shift+click adds it to the selection or takes
 * it out, as Shift+click on the canvas does.
 *
 * The rows are worked out from the level every time they are needed.
 * Nothing is remembered between frames, so undo, redo, delete, paste and
 * loading another level can never leave a row naming an entity that has
 * moved to another index or gone.
 */
#pragma once

#include <stddef.h>  /* size_t */

#include "editor.h"  /* EditorState, EntityType, LevelDef */

/* The search field's widget ID.  9000 and up are level-wide widgets, which
 * editor_finish_field_edit treats as "not a field of the selected entity". */
#define ENTITY_LIST_SEARCH_ID 9900

/*
 * Layout, in pixels.  The panel is: a title bar, the search field, then
 * the rows.  ENTITY_LIST_ROWS_TOP is how far below the panel's top edge
 * the first row starts (before scrolling); every row, type heading or
 * entity, is ENTITY_LIST_ROW_H tall.
 */
#define ENTITY_LIST_ROWS_TOP 56
#define ENTITY_LIST_ROW_H    18

/* Open the list, switch to the Select tool if the Place tool was active
 * (the list is for selecting) and put the caret in the search field. */
void editor_entity_list_open(EditorState *es);

/* Close the list; Level Config and the palette return. */
void editor_entity_list_close(EditorState *es);

/*
 * editor_entity_list_render — Draw the list and act on clicks in it.
 *
 * start_y     : top edge of the panel in window pixels.
 * available_h : its height.
 */
void editor_entity_list_render(EditorState *es, int start_y, int available_h);

/*
 * editor_entity_list_row_text — The text of one entity's row:
 * "Coin #3 (120, 96)".  The position is the top-left corner of the entity
 * as the canvas draws it, in whole world pixels, except that a checkpoint
 * shows its own x and y (the foot of its pole) and a flame its own x (the
 * floor gap it rises from).  A rail rider whose rail is missing has no
 * place on the canvas and reads "Spike Block #2 (no rail)".
 */
void editor_entity_list_row_text(const LevelDef *level, EntityType type, int index,
                                 char *out, size_t out_size);

/*
 * editor_entity_list_matches — Whether entity (type, index) is listed for
 * the search text `filter`.
 *
 * The text is split into words at spaces, commas, brackets and '#', and every
 * word has to fit:
 *   - a whole number fits an entity whose listed x or y is that number
 *     ("96", or "120 96" for both);
 *   - a whole number right after '#' fits the entity with that index,
 *     as the row writes it ("#3" lists Coin #3, Spider #3...);
 *   - any other word fits when the type's name contains it, whatever the
 *     case ("spi" lists Spider, Jumping Spider, Spike Row...).
 * An empty filter lists everything.
 */
int editor_entity_list_matches(const LevelDef *level, EntityType type, int index,
                               const char *filter);

/* The search text in use this frame: what is being typed while the search
 * field has the caret, the text last confirmed otherwise. */
const char *editor_entity_list_filter(const EditorState *es);

/*
 * editor_entity_list_row_of — Which row shows entity (type, index) under
 * the current search text: 0 for the first row, counting the type
 * headings.  -1 when the entity is not listed.  The row's top edge is at
 *   panel top + ENTITY_LIST_ROWS_TOP + row * ENTITY_LIST_ROW_H - scroll
 * where scroll is es->entity_list_scroll.offset.
 */
int editor_entity_list_row_of(const EditorState *es, EntityType type, int index);

/* How many rows the current search text lists, type headings included. */
int editor_entity_list_row_count(const EditorState *es);
