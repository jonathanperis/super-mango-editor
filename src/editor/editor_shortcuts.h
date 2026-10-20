/*
 * editor_shortcuts.h — The editor's keyboard and mouse shortcuts, listed once.
 *
 * One table names every shortcut editor_events.c handles: its keys and what
 * it does. The toolbar's tooltips are built from it ("Save (Ctrl+S)"), and
 * F1 shows all of it in a panel over the editor, so a key changed here
 * changes both. editor_events.c still decides what each key does, so a
 * shortcut added or changed there needs its row here too.
 */

#ifndef EDITOR_SHORTCUTS_H
#define EDITOR_SHORTCUTS_H

#include "editor.h"                 /* EditorState */
#include "../input/input_backend.h" /* InputEvent */

/* Rows of the table; the toolbar asks for its tooltips by these. */
typedef enum {
    EDITOR_SHORTCUT_SELECT_TOOL,
    EDITOR_SHORTCUT_PLACE_TOOL,
    EDITOR_SHORTCUT_DELETE_TOOL,
    EDITOR_SHORTCUT_ESCAPE,
    EDITOR_SHORTCUT_DELETE,
    EDITOR_SHORTCUT_RIGHT_CLICK,
    EDITOR_SHORTCUT_NUDGE,
    EDITOR_SHORTCUT_COPY_PASTE,
    EDITOR_SHORTCUT_SELECT_ALL,
    EDITOR_SHORTCUT_DUPLICATE,
    EDITOR_SHORTCUT_UNDO,
    EDITOR_SHORTCUT_REDO,
    EDITOR_SHORTCUT_GRID,
    EDITOR_SHORTCUT_SNAP,
    EDITOR_SHORTCUT_PAN,
    EDITOR_SHORTCUT_ZOOM,
    EDITOR_SHORTCUT_FIT,
    EDITOR_SHORTCUT_PLAYTEST,
    EDITOR_SHORTCUT_PLAYTEST_HERE,
    EDITOR_SHORTCUT_SAVE,
    EDITOR_SHORTCUT_SAVE_AS,
    EDITOR_SHORTCUT_OPEN,
    EDITOR_SHORTCUT_NEW,
    EDITOR_SHORTCUT_RECENT,
    EDITOR_SHORTCUT_RECOVER,
    EDITOR_SHORTCUT_CAMPAIGN,
    EDITOR_SHORTCUT_SHEET,
    EDITOR_SHORTCUT_COUNT
} EditorShortcutId;

/*
 * keys   : what to press, as the sheet shows it ("Ctrl+S").
 * action : what it does, for the sheet.
 * tip    : the toolbar tooltip's words before the keys, or NULL when no
 *          toolbar control has this shortcut.
 * tip_more: words after the keys inside the tooltip's brackets, or NULL.
 */
typedef struct {
    const char *keys;
    const char *action;
    const char *tip;
    const char *tip_more;
} EditorShortcut;

const EditorShortcut *editor_shortcut(EditorShortcutId id);

/* "<tip> (<keys><tip_more>)", e.g. "Save (Ctrl+S)"; the text lives as long
 * as the program (ui_tooltip keeps the pointer). NULL for a row without a
 * tip. */
const char *editor_shortcut_tooltip(EditorShortcutId id);

/* Open or close the F1 sheet, and take the input that belongs to it: F1
 * toggles it; while it is open, Esc or a click closes it and every other
 * key, typed character, click and wheel is swallowed, so nothing under it
 * changes. Returns 1 when the event was the sheet's. */
int editor_shortcut_sheet_event(EditorState *es, const InputEvent *event);

/* Draw the sheet over the whole editor while it is open (UI style colours). */
void editor_shortcut_sheet_render(EditorState *es);

#endif /* EDITOR_SHORTCUTS_H */
