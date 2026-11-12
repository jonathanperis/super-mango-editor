/*
 * editor_entity_list.c — The Entities list (see editor_entity_list.h).
 *
 * The list is immediate-mode like the rest of the editor's panels: each
 * frame it walks the level, decides which entities the search text lists,
 * and draws the rows that are in view.  The level holds a few hundred
 * entities at most (every array has a MAX_* capacity), so the walk is
 * cheap and there is nothing to keep in step with the document.
 */

#include "editor_entity_list.h"

#include <ctype.h>   /* tolower, isdigit */
#include <math.h>    /* roundf */
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* strtol */
#include <string.h>  /* strlen */

#include "canvas.h"              /* canvas_center_on */
#include "editor_session.h"      /* editor_finish_field_edit, editor_set_status */
#include "entity_meta.h"         /* names, counts, selection helpers */
#include "hit_test.h"            /* editor_entity_bounds */
#include "properties_internal.h" /* PROP_X, PROP_W, draw_section_title */
#include "../shared/ui.h"        /* widgets, UIScroll */

/* A search word is cut short at this many characters; no type name or
 * coordinate is longer. */
#define ENTITY_LIST_WORD_MAX 32

/* ------------------------------------------------------------------ */
/* Opening and closing                                                 */
/* ------------------------------------------------------------------ */

void editor_entity_list_open(EditorState *es)
{
    if (!es) return;
    es->entity_list_open = 1;
    /* The Place tool needs the palette, which the list covers. */
    if (es->tool == TOOL_PLACE) es->tool = TOOL_SELECT;
    /* The search field takes the caret when it is next drawn. */
    ui_focus_field(&es->ui, ENTITY_LIST_SEARCH_ID);
}

void editor_entity_list_close(EditorState *es)
{
    if (!es) return;
    es->entity_list_open = 0;
}

/* ------------------------------------------------------------------ */
/* One entity's row: its position and its text                         */
/* ------------------------------------------------------------------ */

/*
 * listed_position — The (x, y) a row shows, in whole world pixels.
 * Returns 0 when the entity has no place on the canvas (a rail rider
 * whose rail is missing).
 */
static int listed_position(const LevelDef *level, EntityType type, int index,
                           int *x, int *y)
{
    EditorRect r;

    if (!editor_entity_bounds(level, type, index, &r)) return 0;
    /* A checkpoint's click target is a box around its pole, and a flame
     * is drawn centred in its gap; show the numbers the Properties panel
     * shows for them instead of a corner of those boxes. */
    if (type == ENT_CHECKPOINT) {
        r.x = level->checkpoints[index].x;
        r.y = level->checkpoints[index].y;
    } else if (type == ENT_BLUE_FLAME) {
        r.x = level->blue_flames[index].x;
    } else if (type == ENT_FIRE_FLAME) {
        r.x = level->fire_flames[index].x;
    }
    *x = (int)roundf(r.x);
    *y = (int)roundf(r.y);
    return 1;
}

void editor_entity_list_row_text(const LevelDef *level, EntityType type, int index,
                                 char *out, size_t out_size)
{
    int x, y;

    if (!out || out_size == 0) return;
    if (listed_position(level, type, index, &x, &y))
        snprintf(out, out_size, "%s #%d (%d, %d)",
                 editor_entity_type_name(type), index, x, y);
    else
        snprintf(out, out_size, "%s #%d (no rail)",
                 editor_entity_type_name(type), index);
}

/* ------------------------------------------------------------------ */
/* Searching                                                           */
/* ------------------------------------------------------------------ */

/* The characters that separate search words. */
static int is_word_separator(char c)
{
    return c == ' ' || c == ',' || c == '(' || c == ')' || c == '#';
}

/*
 * next_word — Copy the next search word of *text into word (lower-cased,
 * at most ENTITY_LIST_WORD_MAX - 1 characters) and move *text past it.
 * *after_hash is 1 when a '#' comes right before the word, as in "#3",
 * the way a row writes an entity's index ("Coin #3").  Returns 0 when no
 * word is left.
 */
static int next_word(const char **text, char *word, int *after_hash)
{
    const char *p = *text;
    int length = 0;

    while (*p && is_word_separator(*p)) p++;
    if (*p == '\0') {
        *text = p;
        return 0;
    }
    *after_hash = p > *text && p[-1] == '#';
    while (*p && !is_word_separator(*p)) {
        if (length < ENTITY_LIST_WORD_MAX - 1)
            word[length++] = (char)tolower((unsigned char)*p);
        p++;
    }
    word[length] = '\0';
    *text = p;
    return 1;
}

/*
 * word_is_number — 1 when word is a whole number such as "96" or "-4",
 * with its value in *value.  "12a", "-" and "1.5" are not: they are
 * looked for in the type's name like any other word.
 */
static int word_is_number(const char *word, long *value)
{
    const char *digits = word[0] == '-' ? word + 1 : word;
    char *end = NULL;

    if (digits[0] == '\0') return 0;
    for (const char *p = digits; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    *value = strtol(word, &end, 10);
    return end && *end == '\0';
}

/* 1 when `name` contains `word`, ignoring case.  word is already lower
 * case (next_word). */
static int name_contains(const char *name, const char *word)
{
    size_t word_length = strlen(word);
    size_t name_length = strlen(name);

    if (word_length > name_length) return 0;
    for (size_t start = 0; start + word_length <= name_length; start++) {
        size_t i = 0;
        while (i < word_length &&
               tolower((unsigned char)name[start + i]) == (unsigned char)word[i])
            i++;
        if (i == word_length) return 1;
    }
    return 0;
}

int editor_entity_list_matches(const LevelDef *level, EntityType type, int index,
                               const char *filter)
{
    char word[ENTITY_LIST_WORD_MAX];
    const char *rest = filter ? filter : "";
    const char *name = editor_entity_type_name(type);
    int x = 0, y = 0;
    int has_position = listed_position(level, type, index, &x, &y);
    int after_hash = 0;

    while (next_word(&rest, word, &after_hash)) {
        long number;
        if (after_hash && word_is_number(word, &number)) {
            /* "#3" is an index: Coin #3, not something at x or y 3. */
            if (number != (long)index) return 0;
        } else if (word_is_number(word, &number)) {
            if (!has_position || (number != (long)x && number != (long)y)) return 0;
        } else if (!name_contains(name, word)) {
            return 0;
        }
    }
    return 1;
}

const char *editor_entity_list_filter(const EditorState *es)
{
    /* While the search field has the caret, its text is still in the
     * widget's edit buffer (ui.h): reading it there filters as you type,
     * without waiting for Enter. */
    if (es->ui.active_id == ENTITY_LIST_SEARCH_ID) return es->ui.edit_buf;
    return es->entity_list_filter;
}

/* ------------------------------------------------------------------ */
/* Walking the rows                                                    */
/* ------------------------------------------------------------------ */

/* How many entities of one type the search text lists. */
static int listed_count(const EditorState *es, EntityType type, const char *filter)
{
    int total = editor_entity_count(&es->level, type);
    int listed = 0;

    for (int i = 0; i < total; i++)
        if (editor_entity_list_matches(&es->level, type, i, filter)) listed++;
    return listed;
}

/*
 * The rows, top to bottom: for each type that has listed entities, in
 * EntityType order, one heading row and then one row per listed entity in
 * index order.  The three functions below all walk that same order.
 */
int editor_entity_list_row_count(const EditorState *es)
{
    const char *filter = editor_entity_list_filter(es);
    int rows = 0;

    for (int t = 0; t < ENT_COUNT; t++) {
        int listed = listed_count(es, (EntityType)t, filter);
        if (listed > 0) rows += 1 + listed;
    }
    return rows;
}

int editor_entity_list_row_of(const EditorState *es, EntityType type, int index)
{
    const char *filter = editor_entity_list_filter(es);
    int row = 0;

    for (int t = 0; t < ENT_COUNT; t++) {
        int total = editor_entity_count(&es->level, (EntityType)t);
        if (listed_count(es, (EntityType)t, filter) == 0) continue;
        row++;   /* the type's heading */
        for (int i = 0; i < total; i++) {
            if (!editor_entity_list_matches(&es->level, (EntityType)t, i, filter)) continue;
            if ((EntityType)t == type && i == index) return row;
            row++;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Drawing and clicks                                                  */
/* ------------------------------------------------------------------ */

/*
 * pick_entity — What a click on an entity's row does.  A plain click makes
 * it the selection and centres the canvas on it; with Shift it is added
 * to the selection or taken out and the view stays where it is, as
 * Shift+click on the canvas works.
 */
static void pick_entity(EditorState *es, EntityType type, int index, int shift)
{
    EditorRect r;

    es->tool = TOOL_SELECT;
    if (shift) {
        /* A full selection left its own message: keep it. */
        if (!editor_selection_toggle(es, type, index)) return;
        editor_set_status(es, editor_selection_count(es) == 1
                          ? "Selected %d entity" : "Selected %d entities",
                          editor_selection_count(es));
        return;
    }
    editor_select_only(es, type, index);
    if (editor_entity_bounds(&es->level, type, index, &r)) canvas_center_on(es, &r);
    editor_set_status(es, "Selected %s %d", editor_entity_type_name(type), index);
}

void editor_entity_list_render(EditorState *es, int start_y, int available_h)
{
    const int x = PROP_X;
    const int rows_top = start_y + ENTITY_LIST_ROWS_TOP;
    const int rows_h = available_h - ENTITY_LIST_ROWS_TOP;
    const char *filter;
    char listed_text[32];
    int open = 1;
    int total_rows, y, listed_entities = 0;

    ui_panel(&es->ui, x, start_y, PROP_W, available_h);

    /* The title bar closes the list when clicked, the way the other
     * sections' title bars fold them. */
    draw_section_title(es, x, start_y, " Entities", &open);
    if (!open) {
        es->ui.mouse_clicked = 0;   /* the click is used up */
        editor_entity_list_close(es);
        return;
    }

    /* The search field.  It is drawn before the rows so that this frame's
     * typing is already in its edit buffer when the rows are filtered. */
    ui_label_tip(&es->ui, x + 8, start_y + 34, "Find:",
                 "A type name, or a coordinate such as 96; Esc closes the list");
    (void)ui_text_field_hint(&es->ui, ENTITY_LIST_SEARCH_ID, x + 48, start_y + 32,
                             PROP_W - 48 - 12, es->entity_list_filter,
                             (int)sizeof(es->entity_list_filter),
                             "type or coordinate", 0);
    /* Level Config scrolls to a field of its own that takes the caret
     * (focus_landed_id, 9000 and up); this one is not in that panel. */
    if (es->ui.focus_landed_id == ENTITY_LIST_SEARCH_ID) es->ui.focus_landed_id = 0;

    if (rows_h <= 0) return;
    filter = editor_entity_list_filter(es);
    total_rows = editor_entity_list_row_count(es);

    /* The scrollbar takes its clicks before the rows can see them. */
    (void)ui_scrollbar_input(&es->ui, &es->entity_list_scroll, x + PROP_W - SCROLLBAR_W,
                             rows_top, SCROLLBAR_W, rows_h, rows_h,
                             total_rows * ENTITY_LIST_ROW_H);
    y = ui_scroll_begin(&es->ui, &es->entity_list_scroll, x, rows_top, PROP_W, rows_h,
                        total_rows * ENTITY_LIST_ROW_H);

    for (int t = 0; t < ENT_COUNT; t++) {
        EntityType type = (EntityType)t;
        int total = editor_entity_count(&es->level, type);
        int listed = listed_count(es, type, filter);
        char text[64];

        if (listed == 0) continue;
        listed_entities += listed;

        /* The type's heading: its name and how many are listed. */
        if (y + ENTITY_LIST_ROW_H > rows_top && y < rows_top + rows_h) {
            snprintf(text, sizeof(text), "%s (%d)", editor_entity_type_name(type), listed);
            ui_label_color(&es->ui, x + 8, y + 2, text, ui_color(&es->ui, UI_COLOR_ACCENT));
        }
        y += ENTITY_LIST_ROW_H;

        for (int i = 0; i < total; i++) {
            int in_view, hovered;
            if (!editor_entity_list_matches(&es->level, type, i, filter)) continue;

            /* Rows scrolled out of the panel are neither drawn nor
             * clickable: the pointer there is on another panel. */
            in_view = y + ENTITY_LIST_ROW_H > rows_top && y < rows_top + rows_h;
            hovered = in_view &&
                      es->ui.mouse_x >= x && es->ui.mouse_x < x + PROP_W - SCROLLBAR_W &&
                      es->ui.mouse_y >= y && es->ui.mouse_y < y + ENTITY_LIST_ROW_H &&
                      es->ui.mouse_y >= rows_top && es->ui.mouse_y < rows_top + rows_h;
            if (in_view) {
                if (editor_is_selected(es, type, i))
                    DrawRectangle(x + 4, y, PROP_W - 8 - SCROLLBAR_W, ENTITY_LIST_ROW_H,
                                  ui_color(&es->ui, UI_COLOR_SELECTED));
                else if (hovered)
                    DrawRectangle(x + 4, y, PROP_W - 8 - SCROLLBAR_W, ENTITY_LIST_ROW_H,
                                  ui_color(&es->ui, UI_COLOR_HOT));
                editor_entity_list_row_text(&es->level, type, i, text, sizeof(text));
                ui_label(&es->ui, x + 20, y + 2, text);
            }
            /*
             * A click on the row.  A drag or a painted row owns the
             * document until its button is released, so nothing is picked
             * then.  The search field's text is settled first, like any
             * field before a command.
             */
            if (hovered && es->ui.mouse_clicked && !es->dragging && !es->painting) {
                es->ui.mouse_clicked = 0;
                if (editor_finish_field_edit(es))
                    pick_entity(es, type, i, (es->input_mods & INPUT_SHIFT) != 0);
            }
            y += ENTITY_LIST_ROW_H;
        }
    }
    ui_scroll_end(&es->ui);
    ui_scrollbar_draw(&es->ui, &es->entity_list_scroll, x + PROP_W - SCROLLBAR_W, rows_top,
                      SCROLLBAR_W, rows_h, rows_h, total_rows * ENTITY_LIST_ROW_H);

    /* How many entities are listed, at the right end of the title bar. */
    snprintf(listed_text, sizeof(listed_text), "%d listed", listed_entities);
    ui_label_color(&es->ui, x + PROP_W - 8 - ui_text_width(&es->ui, listed_text),
                   start_y + 4, listed_text, ui_color(&es->ui, UI_COLOR_TEXT_DIM));

    if (listed_entities == 0)
        ui_label_color(&es->ui, x + 8, rows_top + 2,
                       filter[0] ? "Nothing matches" : "The level has no entities yet",
                       ui_color(&es->ui, UI_COLOR_TEXT_DIM));
}
