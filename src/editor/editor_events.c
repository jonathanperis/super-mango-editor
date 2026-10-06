/* Text edits own printable input before ordinary canvas shortcuts. Native
 * commands and toolbar actions share the same transactional editor helpers. */
#include "editor_events.h"
#include "canvas.h"
#include "editor_clipboard.h"
#include "editor_files.h"
#include "editor_panels.h"
#include "editor_playtest.h"
#include "editor_session.h"
#include "editor_undo_apply.h"
#include "tools.h"

static void editor_history(EditorState *es, int redo)
{
    /* Finish the staged field edit before moving history. Otherwise Undo could
     * change the document while a widget still points at its previous value. */
    Command command;
    if (es->dragging) return;  /* editor_key already explained why */
    if (!editor_finish_field_edit(es)) return;
    if (redo ? redo_pop(es->undo, &command) : undo_pop(es->undo, &command)) {
        editor_apply_undo_command(es, &command, redo ? 0 : 1);
        editor_refresh_dirty(es);
    }
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
    if (es->ui.active_id && !ctrl && key != KEY_ESCAPE && key != KEY_F5) {
        if (key == KEY_BACKSPACE) es->ui.key_backspace = 1;
        if (key == KEY_ENTER || key == KEY_KP_ENTER) es->ui.key_return = 1;
        return;
    }
    if (es->ui.active_id && ctrl && (key == KEY_C || key == KEY_V)) {
        if (key == KEY_C) SetClipboardText(es->ui.edit_buf);
        else {
            const char *text = GetClipboardText(); /* borrowed by raylib; do not free */
            if (text) ui_queue_text_input(&es->ui, text);
        }
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
        case KEY_V:
            if (editor_finish_field_edit(es)) editor_paste_clipboard(es);
            break;
        default:
            break;
        }
        return;
    }
    switch (key) {
    case KEY_ESCAPE:
        if (es->ui.active_id) ui_cancel_active_edit(&es->ui);
        else if (es->tool == TOOL_PLACE || es->tool == TOOL_DELETE) es->tool = TOOL_SELECT;
        else if (es->selection.index >= 0) {
            es->selection.index = -1;
            es->panel_scroll = 0;
        }
        break;
    case KEY_F5:
        editor_play_test(es);
        break;
    case KEY_G:
        if (editor_finish_field_edit(es)) es->show_grid ^= 1;
        break;
    case KEY_DELETE:
        if (es->selection.index >= 0 && editor_finish_field_edit(es)) tools_delete_selected(es);
        break;
    case KEY_ONE:
        if (editor_finish_field_edit(es)) es->tool = TOOL_SELECT;
        break;
    case KEY_TWO:
        if (editor_finish_field_edit(es)) es->tool = TOOL_PLACE;
        break;
    case KEY_THREE:
        if (editor_finish_field_edit(es)) es->tool = TOOL_DELETE;
        break;
    case KEY_BACKSPACE:
        es->ui.key_backspace = 1;
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
 *   wheel        : pan left/right (the level is much wider than tall)
 *   Shift+wheel  : pan up/down (needed at 3x/5x, where the floor is
 *                  below the visible area)
 *   Ctrl+wheel   : step through the zoom presets, keeping the point under
 *                  the cursor in place
 */
static void editor_canvas_wheel(EditorState *es, const InputEvent *event)
{
    /* One wheel notch pans 48 canvas pixels; divide by zoom for world px. */
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    float step = event->wheel * 48.0f / zoom;

    if (event->mods & INPUT_CTRL) {
        /* These exact integer presets are assigned by startup, the
         * toolbar and this wheel handler; zoom is not accumulated. */
        static const float zooms[] = {1, 2, 3, 5};
        int index = 1;
        for (int i = 0; i < 4; i++)
            if (es->camera.zoom == zooms[i]) {
                index = i;
                break;
            }
        if (event->wheel > 0) index = (index+1)%4;
        else if (event->wheel < 0) index = (index+3)%4;
        canvas_set_zoom(es, zooms[index], event->x, event->y);
    } else if (event->mods & INPUT_SHIFT) {
        es->camera.y -= step;
        canvas_clamp_camera(es);
    } else {
        es->camera.x -= step;
        canvas_clamp_camera(es);
    }
}

void editor_handle_event(EditorState *es, const InputEvent *event)
{
    float wx = 0, wy = 0;
    /* Input already maps window pixels to the logical editor canvas. Tools
     * need world coordinates; canvas_screen_to_world undoes zoom and adds
     * the camera scroll in both axes. */
    if (event->type >= INPUT_MOUSE_DOWN && event->type <= INPUT_WHEEL)
        canvas_screen_to_world(es, event->x, event->y, &wx, &wy);
    switch (event->type) {
    case INPUT_QUIT:
        if (editor_confirm_discard_changes(es, "quit")) {
            editor_retire_current_recovery(es);
            if (es->playing) editor_stop_play(es);
            es->running = 0;
        }
        break;
    case INPUT_KEY_DOWN:
        editor_key(es, event);
        break;
    case INPUT_TEXT:
        ui_queue_text_input(&es->ui, event->text);
        break;
    case INPUT_MOUSE_DOWN:
        if (event->button == MOUSE_BUTTON_LEFT) {
            es->mouse_down = es->ui.mouse_clicked = 1;
            if (canvas_contains(event->x, event->y) && editor_finish_field_edit(es))
                tools_mouse_down(es, wx, wy);
        } else if (event->button == MOUSE_BUTTON_RIGHT) {
            es->mouse_right_down = 1;
            if (canvas_contains(event->x, event->y) && editor_finish_field_edit(es))
                tools_right_click(es, wx, wy);
        }
        break;
    case INPUT_MOUSE_UP:
        if (event->button == MOUSE_BUTTON_LEFT) {
            es->mouse_down = 0;
            tools_mouse_up(es, wx, wy);
        }
        else if (event->button == MOUSE_BUTTON_RIGHT) es->mouse_right_down = 0;
        break;
    case INPUT_MOUSE_MOVE:
        if (es->mouse_down) tools_mouse_drag(es,wx,wy);
        break;
    case INPUT_WHEEL:
        if (editor_handle_side_panel_scroll(es,event->x,event->y,(int)event->wheel)) break;
        if (canvas_contains(event->x, event->y)) editor_canvas_wheel(es, event);
        break;
    default: break;
    }
}
