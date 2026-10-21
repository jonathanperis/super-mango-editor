/* Text edits own printable input before ordinary canvas shortcuts. Native
 * commands and toolbar actions share the same transactional editor helpers. */
#include "editor_events.h"
#include "canvas.h"
#include "editor_campaign.h"
#include "editor_clipboard.h"
#include "editor_files.h"
#include "editor_recovery.h"
#include "editor_panels.h"
#include "editor_playtest.h"
#include "editor_session.h"
#include "editor_shortcuts.h"
#include "editor_undo_apply.h"
#include "../shared/geometry.h"  /* float_same_value */
#include "tools.h"
#include "entity_meta.h"   /* editor_select_none */

#include <ctype.h>    /* tolower */
#include <stdio.h>    /* snprintf */
#include <string.h>   /* strlen */

/*
 * The system clipboard, through raylib.  Test builds use a buffer of their
 * own instead: the headless raylib the tests run on has no clipboard.
 */
#ifdef MANGO_TESTING
static char s_test_clipboard[UI_EDIT_BUFFER_SIZE];

const char *editor_test_clipboard(void) { return s_test_clipboard; }

void editor_test_set_clipboard(const char *text)
{
    snprintf(s_test_clipboard, sizeof(s_test_clipboard), "%s", text ? text : "");
}

static void clipboard_set(const char *text) { editor_test_set_clipboard(text); }
static const char *clipboard_get(void) { return s_test_clipboard; }
#else
static void clipboard_set(const char *text) { SetClipboardText(text); }
/* Borrowed from raylib; do not free. */
static const char *clipboard_get(void) { return GetClipboardText(); }
#endif

/*
 * raylib copies each dropped path into a MAX_FILEPATH_LENGTH buffer and
 * cuts a longer path short without saying so.  The macro is private to
 * raylib's rcore.c, which sets it to 260 (MAX_PATH) on Windows and 4096
 * elsewhere; a path that fills the buffer may have lost its end, so it is
 * refused rather than opening some other file.
 */
#ifdef _WIN32
#define EDITOR_DROP_PATH_LIMIT 260
#else
#define EDITOR_DROP_PATH_LIMIT 4096
#endif

/*
 * Files dropped on the window, through raylib.  LoadDroppedFiles does not
 * copy anything: it lends raylib's own list, which UnloadDroppedFiles
 * frees and which the next drop frees and replaces (WindowDropCallback).
 * Any event poll can deliver that next drop, and the in-window prompt
 * polls every frame it shows, so the list is released before anything
 * that can ask a question.
 *
 * Test builds keep a list of their own that behaves the same way, since
 * the headless raylib the tests run on has no window to drop onto: a new
 * drop overwrites the paths of one still held (so a stale pointer reads
 * the new path, as reused memory would), and releasing a list that a
 * newer drop already replaced counts as a misuse (raylib would free it
 * twice).
 */
#ifdef MANGO_TESTING
#define TEST_DROP_MAX 4
static char s_test_drop_text[TEST_DROP_MAX][EDITOR_DROP_PATH_LIMIT];
static char *s_test_drop_paths[TEST_DROP_MAX];
static unsigned int s_test_drop_count;
static unsigned int s_test_drop_generation;  /* bumped by every drop */
static unsigned int s_test_drop_taken;       /* the generation last taken */
static int s_test_drop_misuses;

void editor_test_drop_files(const char *const *paths, unsigned int count)
{
    if (!paths) count = 0;
    if (count > TEST_DROP_MAX) count = TEST_DROP_MAX;
    for (unsigned int i = 0; i < count; i++) {
        snprintf(s_test_drop_text[i], sizeof(s_test_drop_text[i]), "%s", paths[i]);
        s_test_drop_paths[i] = s_test_drop_text[i];
    }
    s_test_drop_count = count;
    s_test_drop_generation++;
}

int editor_test_drop_misuses(void) { return s_test_drop_misuses; }

static int dropped_files_take(FilePathList *files)
{
    if (s_test_drop_count == 0) return 0;
    files->count = s_test_drop_count;
    files->paths = s_test_drop_paths;
    s_test_drop_taken = s_test_drop_generation;
    return 1;
}

static void dropped_files_release(FilePathList files)
{
    (void)files;
    if (s_test_drop_count == 0 || s_test_drop_taken != s_test_drop_generation)
        s_test_drop_misuses++;
    s_test_drop_count = 0;
}
#else
static int dropped_files_take(FilePathList *files)
{
    if (!IsFileDropped()) return 0;
    *files = LoadDroppedFiles();
    return 1;
}

static void dropped_files_release(FilePathList files) { UnloadDroppedFiles(files); }
#endif

/* ".toml" at the end of path, in any case (Windows and macOS file names
 * are usually case-insensitive, so LEVEL.TOML is the same kind of file). */
static int path_is_toml(const char *path)
{
    static const char suffix[] = ".toml";
    size_t length = strlen(path), suffix_length = sizeof(suffix) - 1;
    if (length <= suffix_length) return 0;
    for (size_t i = 0; i < suffix_length; i++)
        if (tolower((unsigned char)path[length - suffix_length + i]) != suffix[i])
            return 0;
    return 1;
}

/* Open the one level in a drop, the way Ctrl+1..5 opens a recent one.
 * path is the editor's own copy of it, or NULL when the drop was not
 * exactly one .toml file. */
static void open_dropped_level(EditorState *es, const char *path)
{
    if (es->playing) {
        editor_set_status(es, "Playtest running: click Stop or close the game window to edit");
        return;
    }
    /* A drag owns the document until the button is released (see
     * editor_key); a load would pull the level out from under it. */
    if (es->dragging) {
        editor_set_status(es, "Release the mouse to finish the move first (Esc cancels it)");
        return;
    }
    if (!path) {
        editor_set_status(es, "Drop one .toml level to open it");
        return;
    }
    if (strlen(path) >= EDITOR_DROP_PATH_LIMIT - 1) {
        editor_set_status(es, "Dropped path is too long to open safely: use Ctrl+O");
        return;
    }
    /* The Campaign view covers the canvas; close it first, as Ctrl+M or
     * Esc would.  With unsaved campaign edits the first close only arms
     * (editor_campaign_close), so a second drop discards them. */
    if (es->campaign && !editor_campaign_close(es, 0)) {
        editor_set_status(es, "Unsaved campaign changes: Save them, or drop the level "
                          "again to discard them");
        return;
    }
    /* editor_load_level reports success or the reason it failed. */
    if (editor_confirm_discard_changes(es, "open the dropped level"))
        (void)editor_load_level(es, path);
}

void editor_handle_dropped_files(EditorState *es)
{
    FilePathList files = {0};
    /* One byte past raylib's buffer: a copy of a path that filled it is
     * still long enough for open_dropped_level to refuse. */
    static char path[EDITOR_DROP_PATH_LIMIT + 1];
    int is_level;

    if (!dropped_files_take(&files)) return;
    is_level = files.count == 1 && files.paths[0] && path_is_toml(files.paths[0]);
    if (is_level) snprintf(path, sizeof(path), "%s", files.paths[0]);
    /* Before open_dropped_level, which may ask about unsaved changes: the
     * prompt's frames poll events, and a drop among them frees this list
     * (see dropped_files_take). */
    dropped_files_release(files);
    open_dropped_level(es, is_level ? path : NULL);
}

/*
 * note_redone_place — Remember an entity a redone CMD_PLACE put back, so a
 * redone group paste can select all of them, as the paste itself did.
 * Each redo selects only the entity it inserted, and a later insert into
 * the same array moves the ones after it up a slot, so earlier entries in
 * the list follow that shift first.
 */
static void note_redone_place(Selection *placed, int *count, const Command *command)
{
    if (command->type != CMD_PLACE || *count >= EDITOR_MAX_SELECTION) return;
    for (int i = 0; i < *count; i++)
        if (placed[i].type == (EntityType)command->entity_type &&
            placed[i].index >= command->entity_index)
            placed[i].index++;
    placed[*count].type = (EntityType)command->entity_type;
    placed[*count].index = command->entity_index;
    (*count)++;
}

/* The status bar's account of one history step, in the Campaign view's
 * words ("Undid: moved levels/c.toml"): "moved Coin", "placed Coin (3)"
 * for a group of one type, "deleted 4 entities" for a mixed one, or
 * "changed level settings". A step whose entries did different things (a
 * paste that moves the one Last Star and places the rest) says "changed". */
static void describe_step(char *out, size_t size, const Command *last,
                          int entries, int one_type, int one_verb)
{
    static const char *const verbs[] = {
        [CMD_PLACE] = "placed", [CMD_DELETE] = "deleted",
        [CMD_MOVE] = "moved", [CMD_PROPERTY] = "changed", [CMD_CONFIG] = "changed"
    };
    const char *verb = one_verb ? verbs[last->type] : "changed";
    if (last->type == CMD_CONFIG) snprintf(out, size, "%s level settings", verb);
    else if (last->entity_type < 0 || last->entity_type >= ENT_COUNT) snprintf(out, size, "%s an entity", verb);
    else if (entries == 1)
        snprintf(out, size, "%s %s", verb, editor_entity_type_name((EntityType)last->entity_type));
    else if (one_type)
        snprintf(out, size, "%s %s (%d)", verb, editor_entity_type_name((EntityType)last->entity_type), entries);
    else snprintf(out, size, "%s %d entities", verb, entries);
}

/* Whether applying this entry (forward for redo, backward for undo) took
 * its entity away. A one-of-a-kind entity (Last Star, Player Spawn) only
 * ever moves, so it always survives. */
static int entry_removes(const Command *c, int redo)
{
    if (c->type == CMD_CONFIG || c->entity_type < 0 || c->entity_type >= ENT_COUNT) return 1;
    if (editor_entity_type_is_singleton((EntityType)c->entity_type)) return 0;
    return (c->type == CMD_PLACE && !redo) || (c->type == CMD_DELETE && redo);
}

/* After a step, bring an entity it changed into view when it is off the
 * canvas, centred as a validation message's Show does: the last one the
 * step left in the level (an entity it took away has nothing to show).
 * Its index still names it: a step that both removes and keeps entities
 * is a paste or its undo, where only one-of-a-kind entities (index 0) are
 * kept, and every other kind of step keeps all of its entities. */
static void reveal_step(EditorState *es, const Command *shown, int redo)
{
    EditorRect r;
    if (!shown || entry_removes(shown, redo)) return;
    EntityType type = (EntityType)shown->entity_type;
    int singleton = editor_entity_type_is_singleton(type);
    const Command *last = shown;
    if (!editor_entity_bounds(&es->level, type, singleton ? 0 : last->entity_index, &r)) return;
    if (!canvas_shows(es, &r)) canvas_center_on(es, &r);
}

static void editor_history(EditorState *es, int redo)
{
    /* Finish the staged field edit before moving history. Otherwise Undo could
     * change the document while a widget still points at its previous value. */
    static Selection placed[EDITOR_MAX_SELECTION];
    int placed_count = 0;
    Command command;
    int group;
    if (es->dragging) return;  /* editor_key already explained why */
    if (!editor_finish_field_edit(es)) return;
    if (!(redo ? redo_pop(es->undo, &command) : undo_pop(es->undo, &command))) {
        editor_set_status(es, "Nothing to %s", redo ? "redo" : "undo");
        return;
    }
    /*
     * One step may be a group of entries (a multi-entity move, delete or
     * paste; see undo.h).  Keep popping while the next entry belongs to the
     * same group.  Undo meets the entries newest first and redo oldest
     * first, so array positions shift back exactly as they shifted forward.
     */
    group = command.group;
    int entries = 1, one_type = 1, first_type = command.entity_type;
    int one_verb = 1;
    CommandType first_verb = command.type;
    /* The entity to show afterwards: the last one this step left in place. */
    Command shown = {0};
    int have_shown = 0;
    editor_apply_undo_command(es, &command, redo ? 0 : 1);
    if (redo) note_redone_place(placed, &placed_count, &command);
    if (!entry_removes(&command, redo)) { shown = command; have_shown = 1; }
    while (group != 0 &&
           (redo ? redo_top_group(es->undo) : undo_top_group(es->undo)) == group &&
           (redo ? redo_pop(es->undo, &command) : undo_pop(es->undo, &command))) {
        editor_apply_undo_command(es, &command, redo ? 0 : 1);
        if (redo) note_redone_place(placed, &placed_count, &command);
        entries++;
        one_type &= command.entity_type == first_type;
        one_verb &= command.type == first_verb;
        if (!entry_removes(&command, redo)) { shown = command; have_shown = 1; }
    }
    /* A redone group paste or duplicate selects every copy again. */
    if (placed_count > 1) (void)editor_select_items(es, placed, placed_count);
    editor_refresh_dirty(es);
    reveal_step(es, have_shown ? &shown : NULL, redo);
    char what[96];
    describe_step(what, sizeof(what), &command, entries, one_type, one_verb);
    editor_set_status(es, "%s: %s", redo ? "Redid" : "Undid", what);
}

static void editor_key(EditorState *es, const InputEvent *event)
{
    int key = event->key;
    int ctrl = (event->mods & (INPUT_CTRL | INPUT_SUPER)) != 0;
    int shift = (event->mods & INPUT_SHIFT) != 0;
    /* While an entity is being dragged, the drag owns the document: Undo,
     * Delete or Paste would shift array slots under it.  Only Esc (cancel
     * the move) acts until the button is released.  Modifier keys stay
     * silent because Shift is how a drag snaps to the grid. */
    if (es->dragging) {
        if (key == KEY_ESCAPE) tools_cancel_drag(es);
        else if (key != KEY_LEFT_SHIFT && key != KEY_RIGHT_SHIFT &&
                 key != KEY_LEFT_CONTROL && key != KEY_RIGHT_CONTROL &&
                 key != KEY_LEFT_ALT && key != KEY_RIGHT_ALT &&
                 key != KEY_LEFT_SUPER && key != KEY_RIGHT_SUPER)
            editor_set_status(es, "Release the mouse to finish the move first (Esc cancels it)");
        return;
    }
    /* An active text field owns ordinary typing. The digit '2' in a field
     * must not also select the Place tool. Modifiers come from this event. */
    /* An open dropdown list owns the keys that work it: Up / Down,
     * Home / End, Enter, Tab and Esc.  Other keys are ignored rather than
     * reaching the canvas under the list. */
    if (es->ui.dropdown_open_id && !es->ui.active_id && !ctrl) {
        switch (key) {
        case KEY_UP:    ui_dropdown_key(&es->ui, UI_KEY_UP);   break;
        case KEY_DOWN:  ui_dropdown_key(&es->ui, UI_KEY_DOWN); break;
        case KEY_HOME:  ui_dropdown_key(&es->ui, UI_KEY_HOME); break;
        case KEY_END:   ui_dropdown_key(&es->ui, UI_KEY_END);  break;
        case KEY_ENTER: case KEY_KP_ENTER: ui_dropdown_key(&es->ui, UI_KEY_PICK); break;
        case KEY_TAB:   ui_focus_next(&es->ui, shift ? -1 : 1); break;
        case KEY_ESCAPE: es->ui.dropdown_open_id = 0;           break;
        default: break;
        }
        return;
    }
    if (es->ui.active_id && !ctrl && key != KEY_ESCAPE && key != KEY_F5) {
        /* Caret keys act at once, in the order they were pressed, so
         * "type, Left, type" in one frame lands where it should. */
        /* Shift with a caret key selects what the caret passes over. */
        switch (key) {
        case KEY_BACKSPACE: ui_edit_key(&es->ui, UI_KEY_BACKSPACE, 0); break;
        case KEY_DELETE:    ui_edit_key(&es->ui, UI_KEY_DELETE, 0);    break;
        case KEY_LEFT:      ui_edit_key(&es->ui, UI_KEY_LEFT, shift);  break;
        case KEY_RIGHT:     ui_edit_key(&es->ui, UI_KEY_RIGHT, shift); break;
        case KEY_HOME:      ui_edit_key(&es->ui, UI_KEY_HOME, shift);  break;
        case KEY_END:       ui_edit_key(&es->ui, UI_KEY_END, shift);   break;
        /* A number field steps its value (Shift: ten steps). */
        case KEY_UP:        ui_edit_key(&es->ui, UI_KEY_UP, shift);    break;
        case KEY_DOWN:      ui_edit_key(&es->ui, UI_KEY_DOWN, shift);  break;
        case KEY_TAB:       ui_focus_next(&es->ui, shift ? -1 : 1); break;
        case KEY_ENTER: case KEY_KP_ENTER:
            /* Shift+Enter starts a new line in a field that keeps them
             * (the level description); Enter alone confirms. */
            if (shift && es->ui.edit_multiline) ui_queue_text_input(&es->ui, "\n");
            else es->ui.key_return = 1;
            break;
        default: break;
        }
        return;
    }
    /* A validation or load-problem row clicked last: Ctrl+C copies its
     * text (even when the click put the caret in the field it is about).
     * Text selected in that field since then is the newer choice, so it is
     * left to the field branch below, as Ctrl+X already is. */
    char field_selection[UI_EDIT_BUFFER_SIZE];
    int field_has_selection = es->ui.active_id &&
        ui_edit_selected_text(&es->ui, field_selection, sizeof(field_selection)) > 0;
    if (ctrl && key == KEY_C && es->focus_area == EDITOR_FOCUS_MESSAGES &&
        es->selected_message[0] && !field_has_selection) {
        clipboard_set(es->selected_message);
        editor_set_status(es, "Copied: %s", es->selected_message);
        return;
    }
    /* Ctrl with a field active edits text: copy, cut and paste act on the
     * selection, Ctrl+A selects everything, Ctrl+Left / Right jump a word
     * (with Shift, selecting it). */
    if (es->ui.active_id && ctrl &&
        (key == KEY_C || key == KEY_X || key == KEY_V || key == KEY_A ||
         key == KEY_LEFT || key == KEY_RIGHT)) {
        char selected[UI_EDIT_BUFFER_SIZE];
        switch (key) {
        case KEY_C: case KEY_X:
            if (ui_edit_selected_text(&es->ui, selected, sizeof(selected)) == 0) {
                editor_set_status(es, "Nothing selected to %s: Shift+arrows, a drag or "
                                  "Ctrl+A select text", key == KEY_C ? "copy" : "cut");
                break;
            }
            clipboard_set(selected);
            if (key == KEY_X) ui_edit_delete_selection(&es->ui);
            break;
        case KEY_V: {
            const char *text = clipboard_get();
            if (text) ui_queue_text_input(&es->ui, text);   /* replaces the selection */
            break;
        }
        case KEY_A:     ui_edit_key(&es->ui, UI_KEY_SELECT_ALL, 0);     break;
        case KEY_LEFT:  ui_edit_key(&es->ui, UI_KEY_WORD_LEFT, shift);  break;
        case KEY_RIGHT: ui_edit_key(&es->ui, UI_KEY_WORD_RIGHT, shift); break;
        default: break;
        }
        return;
    }
    /* The Campaign view covers the canvas: a level shortcut there would
     * change a document the designer cannot see.  It takes Esc (cancel a
     * name being typed, else close), Ctrl+S (save the campaign), Ctrl+M
     * (close) and the undo keys only.  Those replay the view's own history
     * (editor_campaign.c); the level's undo stack, reached further down,
     * is out of their way until the view closes. */
    if (es->campaign) {
        if (key == KEY_ESCAPE && es->ui.active_id)
            ui_cancel_active_edit(&es->ui);
        else if (key == KEY_ESCAPE || (ctrl && key == KEY_M))
            (void)editor_campaign_close(es, 0);
        else if (ctrl && key == KEY_S)
            (void)editor_campaign_save(es);
        else if (ctrl && key == KEY_Z && !shift)
            (void)editor_campaign_undo(es);
        else if (ctrl && (key == KEY_Z || key == KEY_Y))
            (void)editor_campaign_redo(es);
        else if (key != KEY_LEFT_SHIFT && key != KEY_RIGHT_SHIFT &&
                 key != KEY_LEFT_CONTROL && key != KEY_RIGHT_CONTROL &&
                 key != KEY_LEFT_ALT && key != KEY_RIGHT_ALT &&
                 key != KEY_LEFT_SUPER && key != KEY_RIGHT_SUPER)
            editor_set_status(es, "Campaign view: Esc or Close returns to the level");
        return;
    }
    if (ctrl) {
        switch (key) {
        case KEY_S:
            if (shift) (void)editor_save_current_level_as(es);
            else (void)editor_save_current_level(es);
            break;
        case KEY_O:
            if (editor_confirm_discard_changes(es, "open another level")) editor_open_level_file(es);
            break;
        case KEY_N:
            if (editor_confirm_discard_changes(es, "create a new level")) editor_reset_new_level(es);
            break;
        case KEY_R:
            if (!editor_finish_field_edit(es)) break;
            if (es->recovery_entry_count) {
                int selected = editor_choose_recovery(es);
                uint64_t id = selected >= 0 ? es->pending_recovery_id : 0;
                if (id && editor_confirm_discard_changes(es, "recover autosave"))
                    (void)editor_recover_entry_by_id(es, id);
                else
                    es->pending_recovery_id = 0;
            } else
                editor_set_status(es, "Recovery copy not found");
            break;
        case KEY_ONE: case KEY_TWO: case KEY_THREE: case KEY_FOUR: case KEY_FIVE: {
            int index = key-KEY_ONE;
            if (index < es->recent_file_count && editor_confirm_discard_changes(es, "open a recent level"))
                if (editor_load_level(es, es->recent_files[index])) editor_set_status(es, "Failed to load recent file");
            break;
        }
        case KEY_Z:
            editor_history(es, shift);
            break;
        case KEY_Y:
            editor_history(es, 1);
            break;
        case KEY_C:
            editor_copy_selected(es);
            break;
        case KEY_X:
            /* Cut: copy, then delete the selection. */
            if (es->selection.index >= 0 && editor_finish_field_edit(es)) {
                editor_copy_selected(es);
                tools_delete_selected(es);
            }
            break;
        case KEY_A:
            if (editor_finish_field_edit(es)) editor_select_all(es);
            break;
        case KEY_V:
            if (editor_finish_field_edit(es)) editor_paste_clipboard(es);
            break;
        case KEY_D:
            if (editor_finish_field_edit(es)) editor_duplicate_selection(es);
            break;
        case KEY_M:
            (void)editor_campaign_open(es, NULL);
            break;
        default:
            break;
        }
        return;
    }
    switch (key) {
    case KEY_ESCAPE:
        if (es->ui.active_id) ui_cancel_active_edit(&es->ui);
        else if (es->box_selecting) tools_cancel_drag(es);
        else if (es->tool == TOOL_PLACE || es->tool == TOOL_DELETE) es->tool = TOOL_SELECT;
        else if (es->selection.index >= 0) {
            editor_select_none(es);
        }
        break;
    case KEY_F5:
        /* Shift+F5: playtest from the selected checkpoint or the mouse. */
        if (shift) editor_play_test_here(es);
        else editor_play_test(es);
        break;
    case KEY_G:
        if (editor_finish_field_edit(es)) es->show_grid ^= 1;
        break;
    case KEY_F:
        /* Frame the whole level. */
        canvas_zoom_to_fit(es);
        if (canvas_zoom_index(es->camera.zoom) >= 0)
            editor_set_status(es, "Zoom to fit: %s",
                              canvas_zoom_label(canvas_zoom_index(es->camera.zoom)) + 6);
        else
            editor_set_status(es, "Zoom to fit: %d%%", (int)(es->camera.zoom * 100.0f + 0.5f));
        break;
    case KEY_SPACE:
        /* Held, a left drag on the canvas slides the view (see below). */
        es->space_held = 1;
        break;
    case KEY_S:
        if (editor_finish_field_edit(es)) {
            es->snap_to_grid ^= 1;
            editor_set_status(es, es->snap_to_grid
                ? "Snap to grid on: placing and dragging use the 48 px grid (Shift: free)"
                : "Snap to grid off (hold Shift to snap)");
        }
        break;
    case KEY_BACKSPACE:
        /* Backspace too deletes: many laptop keyboards (Macs) have no Delete
         * key.  Inside a text field it edits text instead (handled above).
         * But right after Enter commits a field, a Backspace still meant
         * for that field would delete the selection, so Backspace deletes
         * only while the canvas has the focus (the last click was on it). */
        if (es->selection.index < 0) break;
        if (es->focus_area != EDITOR_FOCUS_CANVAS) {
            editor_set_status(es, "Backspace deletes on the canvas: click it first, "
                              "or press Delete");
            break;
        }
        if (editor_finish_field_edit(es)) tools_delete_selected(es);
        break;
    case KEY_DELETE:
        if (es->selection.index >= 0 && editor_finish_field_edit(es)) tools_delete_selected(es);
        break;
    case KEY_LEFT: case KEY_RIGHT: case KEY_UP: case KEY_DOWN: {
        /* Nudge the selection 1 px, or 16 px with Shift. */
        float step = shift ? NUDGE_LARGE_PX : 1.0f;
        float dx = key == KEY_LEFT ? -step : key == KEY_RIGHT ? step : 0.0f;
        float dy = key == KEY_UP ? -step : key == KEY_DOWN ? step : 0.0f;
        if (es->selection.index >= 0 && editor_finish_field_edit(es))
            tools_nudge_selection(es, dx, dy);
        break;
    }
    case KEY_ONE:
        if (editor_finish_field_edit(es)) es->tool = TOOL_SELECT;
        break;
    case KEY_TWO:
        if (editor_finish_field_edit(es)) es->tool = TOOL_PLACE;
        break;
    case KEY_THREE:
        if (editor_finish_field_edit(es)) es->tool = TOOL_DELETE;
        break;
    case KEY_ENTER:
        es->ui.key_return = 1;
        break;
    default:
        break;
    }
}

/*
 * editor_canvas_wheel — Zoom or pan the canvas with the mouse wheel.
 *
 *   wheel        : pan left/right (the level is much wider than tall);
 *                  a trackpad's sideways swipe pans left/right too
 *   Shift+wheel  : pan up/down (needed at 3x/5x, where the floor is
 *                  below the visible area).  macOS delivers Shift+wheel
 *                  as horizontal scrolling, so either axis counts here.
 *   Ctrl+wheel   : step through the zoom presets (1/4x to 5x, stopping
 *                  at either end), keeping the point under the cursor in
 *                  place
 */
static void editor_canvas_wheel(EditorState *es, const InputEvent *event)
{
    /* One wheel notch pans 48 canvas pixels; divide by zoom for world px. */
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    float step = event->wheel * 48.0f / zoom;

    if (event->mods & INPUT_CTRL) {
        /* Zoom is one of the canvas's presets (startup, the toolbar and
         * this handler set them), or below them all after Fit on a very
         * long level; step through the presets. */
        const int last = CANVAS_ZOOM_COUNT - 1;
        int index = canvas_zoom_index(es->camera.zoom);
        /* Below every preset (Fit on a very long level): the next step in
         * is the smallest preset, and out stays put. */
        int below = index < 0 && es->camera.zoom < canvas_zoom_preset(0);
        if (index < 0) index = below ? -1 : canvas_zoom_index(1.0f);
        /*
         * A mouse wheel reports whole notches (1.0), but a trackpad or a
         * pinch reports many small fractions.  Add them up and step one
         * preset each time a whole notch has gathered; turning the other
         * way starts a fresh count.
         */
        if ((event->wheel > 0.0f) != (es->zoom_wheel_accum > 0.0f))
            es->zoom_wheel_accum = 0.0f;
        es->zoom_wheel_accum += event->wheel;
        if (es->zoom_wheel_accum >= 1.0f) {
            index++;
            es->zoom_wheel_accum = 0.0f;
        } else if (es->zoom_wheel_accum <= -1.0f) {
            index--;
            es->zoom_wheel_accum = 0.0f;
        }
        /* Stop at the ends instead of wrapping: scrolling past 5x must
         * not suddenly jump back to a quarter. */
        if (index < 0) {
            if (below) return;   /* already smaller than every preset */
            index = 0;
        }
        if (index > last) index = last;
        if (!float_same_value(canvas_zoom_preset(index), es->camera.zoom))
            canvas_set_zoom(es, canvas_zoom_preset(index), event->x, event->y);
    } else if (event->mods & INPUT_SHIFT) {
        float amount = event->wheel != 0.0f ? event->wheel : event->wheel_x;
        es->camera.y -= amount * 48.0f / zoom;
        canvas_clamp_camera(es);
    } else {
        es->camera.x -= step + event->wheel_x * 48.0f / zoom;
        canvas_clamp_camera(es);
    }
}

void editor_handle_event(EditorState *es, const InputEvent *event)
{
    float wx = 0, wy = 0;
    /* Input already maps window pixels to the logical editor canvas. Tools
     * need world coordinates; canvas_screen_to_world undoes zoom and adds
     * the camera scroll in both axes. */
    if (event->type >= INPUT_MOUSE_DOWN && event->type <= INPUT_WHEEL) {
        canvas_screen_to_world(es, event->x, event->y, &wx, &wy);
        /* Remember Shift/Ctrl/Alt as this mouse event saw them: snapping,
         * selection and cycling clicks read them from here. */
        es->input_mods = event->mods;
    }

    /* A release always ends a pan, even when an overlay below (a running
     * playtest, the F1 list) takes the event: otherwise Space or a pan
     * started before it would stay armed afterwards. */
    if (event->type == INPUT_KEY_UP && event->key == KEY_SPACE) es->space_held = 0;
    if (event->type == INPUT_MOUSE_UP && (es->playing || es->shortcut_sheet_open)) es->panning = 0;

    /*
     * While a playtest runs the editor shows only the "Playing" overlay and
     * the game is running a snapshot of this level.  Edits now would change
     * a document the designer cannot see, so input is limited to quitting
     * and the overlay's Stop button (which reads ui.mouse_clicked).
     */
    if (es->playing && event->type != INPUT_QUIT) {
        if (event->type == INPUT_MOUSE_DOWN && event->button == MOUSE_BUTTON_LEFT) {
            es->ui.mouse_clicked = 1;
        } else if (event->type == INPUT_MOUSE_UP) {
            es->mouse_down = 0;  /* a press that started the playtest ends */
            es->mouse_right_down = 0;
        } else if (event->type == INPUT_KEY_DOWN) {
            editor_set_status(es, "Playtest running: click Stop or close the game window to edit");
        }
        return;
    }
    /* The F1 shortcut list, while open, takes the keys and clicks. */
    if (editor_shortcut_sheet_event(es, event)) return;

    switch (event->type) {
    case INPUT_QUIT:
        /* Unsaved campaign edits are asked about first, like Close does. */
        if (!editor_campaign_close(es, 0)) break;
        if (editor_confirm_discard_changes(es, "quit")) {
            editor_retire_current_recovery(es);
            if (es->playing) editor_stop_play(es);
            es->running = 0;
        }
        break;
    case INPUT_KEY_DOWN:
        editor_key(es, event);
        break;
    case INPUT_KEY_UP:
        if (event->key == KEY_SPACE) es->space_held = 0;
        break;
    case INPUT_TEXT:
        ui_queue_text_input(&es->ui, event->text);
        break;
    case INPUT_MOUSE_DOWN:
        if (event->button == MOUSE_BUTTON_LEFT) {
            /* An open dropdown list hangs over the canvas and the panels.
             * The press belongs to the list (pick an option or close it);
             * placing or deleting under it as well would be a surprise. */
            if (ui_press(&es->ui)) break;
            /* The canvas's error banner: go to the problem, nothing else. */
            if (canvas_banner_contains(es, event->x, event->y)) {
                if (editor_finish_field_edit(es)) (void)editor_focus_validation_issue(es, 0);
                break;
            }
            /* Space+drag slides the view instead of using the tool. */
            if (es->space_held && !es->campaign && canvas_contains(event->x, event->y) &&
                es->ui.active_id == 0) {
                es->panning = 1;
                es->pan_button = MOUSE_BUTTON_LEFT;
                es->pan_x = event->x;
                es->pan_y = event->y;
                break;
            }
            es->mouse_down = 1;
            es->focus_area = canvas_contains(event->x, event->y) && !es->campaign
                           ? EDITOR_FOCUS_CANVAS : EDITOR_FOCUS_PANEL;
            /* Over the Campaign view the canvas tools are asleep; its
             * widgets read the click from ui.mouse_clicked. */
            if (canvas_contains(event->x, event->y) && !es->campaign &&
                editor_finish_field_edit(es))
                tools_mouse_down(es, wx, wy);
        } else if (event->button == MOUSE_BUTTON_MIDDLE) {
            /* Middle-drag slides the view, in any tool (not in the middle
             * of a left-button drag, which owns the pointer). */
            if (!es->campaign && !es->mouse_down && !es->panning &&
                canvas_contains(event->x, event->y)) {
                es->panning = 1;
                es->pan_button = MOUSE_BUTTON_MIDDLE;
                es->pan_x = event->x;
                es->pan_y = event->y;
            }
        } else if (event->button == MOUSE_BUTTON_RIGHT) {
            es->mouse_right_down = 1;
            /* Right-click deletes. During a left-button drag that would shift
             * the array under drag_index, so the drag would then overwrite a
             * different entity. Finish (release) the drag first. The same
             * goes for a click under an open dropdown list. */
            if (!es->dragging && es->ui.dropdown_open_id == 0 && !es->campaign &&
                canvas_contains(event->x, event->y) &&
                editor_finish_field_edit(es))
                tools_right_click(es, wx, wy);
        }
        break;
    case INPUT_MOUSE_UP:
        /* Only the button that started the pan ends it; another button's
         * release goes on to the tools as usual. */
        if (es->panning && event->button == es->pan_button) {
            es->panning = 0;
            break;
        }
        if (event->button == MOUSE_BUTTON_LEFT) {
            es->mouse_down = 0;
            tools_mouse_up(es, wx, wy);
        }
        else if (event->button == MOUSE_BUTTON_RIGHT) es->mouse_right_down = 0;
        break;
    case INPUT_MOUSE_MOVE:
        if (es->panning) {
            /* The level follows the pointer: the view moves the other way. */
            float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
            es->camera.x -= (float)(event->x - es->pan_x) / zoom;
            es->camera.y -= (float)(event->y - es->pan_y) / zoom;
            es->pan_x = event->x;
            es->pan_y = event->y;
            canvas_clamp_camera(es);
            break;
        }
        if (es->mouse_down) tools_mouse_drag(es,wx,wy);
        break;
    case INPUT_WHEEL:
        /* The panels take the wheel as a float: a trackpad's small
         * fractions add up there instead of being cut to 0 here. */
        if (editor_handle_side_panel_scroll(es, event->x, event->y, event->wheel)) break;
        if (es->campaign && canvas_contains(event->x, event->y))
            editor_campaign_wheel(es, event->wheel);
        else if (canvas_contains(event->x, event->y)) editor_canvas_wheel(es, event);
        break;
    default: break;
    }
}
