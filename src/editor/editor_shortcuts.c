/*
 * editor_shortcuts.c — The shortcut table, its tooltips and the F1 sheet.
 */

#include "editor_shortcuts.h"
#include "../shared/ui.h"  /* ui_panel, ui_label, ui_color */
#include <stdio.h>         /* snprintf */

/* In the order the sheet lists them: tools and selection, editing, view,
 * playtest, files. Keys as editor_events.c reads them; Ctrl also means
 * Command on macOS there. */
static const EditorShortcut s_shortcuts[EDITOR_SHORTCUT_COUNT] = {
    [EDITOR_SHORTCUT_SELECT_TOOL]   = {"1", "Select tool: select, box-select and drag entities", "Select and move entities", NULL},
    [EDITOR_SHORTCUT_PLACE_TOOL]    = {"2", "Place tool: stamp the palette's entity (drag: a row)", "Place the palette's entity", NULL},
    [EDITOR_SHORTCUT_DELETE_TOOL]   = {"3", "Delete tool: delete what you click", "Delete what you click", NULL},
    [EDITOR_SHORTCUT_ESCAPE]        = {"Esc", "Cancel a field edit, a box or a drag; back to Select; clear the selection", NULL, NULL},
    [EDITOR_SHORTCUT_DELETE]        = {"Delete / Backspace", "Delete the selection (Backspace once the canvas has the focus)", NULL, NULL},
    [EDITOR_SHORTCUT_RIGHT_CLICK]   = {"Right-click", "Delete the entity under the pointer, in any tool", NULL, NULL},
    [EDITOR_SHORTCUT_NUDGE]         = {"Arrows (Shift: 16 px)", "Nudge the selection 1 px", NULL, NULL},
    [EDITOR_SHORTCUT_COPY_PASTE]    = {"Ctrl+C / Ctrl+X / Ctrl+V", "Copy / cut the selection / paste it, offset", NULL, NULL},
    [EDITOR_SHORTCUT_SELECT_ALL]    = {"Ctrl+A", "Select every entity (64 at most)", NULL, NULL},
    [EDITOR_SHORTCUT_DUPLICATE]     = {"Ctrl+D", "Duplicate the selection (the clipboard stays)", NULL, NULL},
    [EDITOR_SHORTCUT_UNDO]          = {"Ctrl+Z", "Undo", NULL, NULL},
    [EDITOR_SHORTCUT_REDO]          = {"Ctrl+Y / Ctrl+Shift+Z", "Redo", NULL, NULL},
    [EDITOR_SHORTCUT_GRID]          = {"G", "Show or hide the grid", "Show the grid", NULL},
    [EDITOR_SHORTCUT_SNAP]          = {"S", "Snap to the 48 px grid on or off (Shift while placing: the other way)", NULL, NULL},
    [EDITOR_SHORTCUT_PAN]           = {"Wheel / Shift+wheel", "Scroll sideways / up and down (or drag: middle button, Space)", NULL, NULL},
    [EDITOR_SHORTCUT_ZOOM]          = {"Ctrl+wheel", "Zoom around the pointer (1/4x, 1/2x, 1x, 2x, 3x, 5x)", "Zoom", "; F fits the level"},
    [EDITOR_SHORTCUT_FIT]           = {"F", "Zoom to fit the whole level", NULL, NULL},
    [EDITOR_SHORTCUT_PLAYTEST]      = {"F5", "Playtest the level", "Playtest", "; Shift+F5 from here"},
    [EDITOR_SHORTCUT_PLAYTEST_HERE] = {"Shift+F5", "Playtest from the selected checkpoint or the pointer", NULL, NULL},
    [EDITOR_SHORTCUT_SAVE]          = {"Ctrl+S", "Save", "Save", NULL},
    [EDITOR_SHORTCUT_SAVE_AS]       = {"Ctrl+Shift+S", "Save As", "Save As", NULL},
    [EDITOR_SHORTCUT_OPEN]          = {"Ctrl+O", "Open a level (or drop a .toml on the window)", "Open", ", or drop a .toml on the window"},
    [EDITOR_SHORTCUT_NEW]           = {"Ctrl+N", "New level", "New level", NULL},
    [EDITOR_SHORTCUT_RECENT]        = {"Ctrl+1 ... Ctrl+5", "Open a recent level", NULL, NULL},
    [EDITOR_SHORTCUT_RECOVER]       = {"Ctrl+R", "Recover or discard an autosave copy", NULL, NULL},
    [EDITOR_SHORTCUT_CAMPAIGN]      = {"Ctrl+M", "Campaign view (Esc or Ctrl+M closes it)", "Campaign view", NULL},
    [EDITOR_SHORTCUT_SHEET]         = {"F1", "Show or hide this list", NULL, NULL},
};

const EditorShortcut *editor_shortcut(EditorShortcutId id)
{
    return id >= 0 && id < EDITOR_SHORTCUT_COUNT ? &s_shortcuts[id] : NULL;
}

const char *editor_shortcut_tooltip(EditorShortcutId id)
{
    /* Built once per row on first use; ui_tooltip borrows the text. */
    static char tips[EDITOR_SHORTCUT_COUNT][96];
    const EditorShortcut *s = editor_shortcut(id);
    if (!s || !s->tip) return NULL;
    if (!tips[id][0])
        snprintf(tips[id], sizeof(tips[id]), "%s (%s%s)", s->tip, s->keys, s->tip_more ? s->tip_more : "");
    return tips[id];
}

int editor_shortcut_sheet_event(EditorState *es, const InputEvent *event)
{
    if (event->type == INPUT_KEY_DOWN && event->key == KEY_F1 && !es->dragging) {
        es->shortcut_sheet_open = !es->shortcut_sheet_open;
        return 1;
    }
    if (!es->shortcut_sheet_open) {
        /* The release of the click that closed the sheet is the sheet's too. */
        if (event->type == INPUT_MOUSE_UP && es->shortcut_sheet_press) {
            es->shortcut_sheet_press = 0;
            return 1;
        }
        return 0;
    }
    switch (event->type) {
    case INPUT_KEY_DOWN:
        if (event->key == KEY_ESCAPE) es->shortcut_sheet_open = 0;
        return 1;
    case INPUT_MOUSE_DOWN:
        es->shortcut_sheet_open = 0;
        es->shortcut_sheet_press = 1;
        return 1;
    case INPUT_TEXT: case INPUT_WHEEL: case INPUT_KEY_UP:
        return 1;
    default:
        return 0;   /* quitting, focus, pointer motion and releases go on */
    }
}

#define SHEET_W     640
#define SHEET_ROW_H 16
#define SHEET_KEYS_W 190

void editor_shortcut_sheet_render(EditorState *es)
{
    if (!es->shortcut_sheet_open) return;
    int h = 36 + EDITOR_SHORTCUT_COUNT * SHEET_ROW_H + 28;
    int x = (EDITOR_W - SHEET_W) / 2, y = (EDITOR_H - h) / 2;
    UIState *ui = &es->ui;
    ui_panel(ui, x, y, SHEET_W, h);
    DrawRectangle(x, y, SHEET_W, 26, ui_color(ui, UI_COLOR_TITLE));
    /* An outline so the panel stands apart from the panels under it. */
    int border = ui->style.border_width > 0 ? ui->style.border_width : 1;
    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)SHEET_W, (float)h},
                         (float)border, ui_color(ui, UI_COLOR_BORDER));
    ui_label(ui, x + 12, y + (26 - TEXT_FONT_SIZE) / 2, "Keyboard and mouse shortcuts");
    for (int i = 0; i < EDITOR_SHORTCUT_COUNT; i++) {
        int row_y = y + 36 + i * SHEET_ROW_H;
        ui_label_color(ui, x + 12, row_y, s_shortcuts[i].keys, ui_color(ui, UI_COLOR_ACCENT));
        ui_label(ui, x + 12 + SHEET_KEYS_W, row_y, s_shortcuts[i].action);
    }
    ui_label_color(ui, x + 12, y + h - 20,
                   "F1 or Esc closes this list. Ctrl also means Command on macOS.",
                   ui_color(ui, UI_COLOR_TEXT_DIM));
}
