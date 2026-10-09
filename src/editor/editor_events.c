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
#include "editor_undo_apply.h"
#include "tools.h"
#include "entity_meta.h"   /* editor_select_none */

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
    if (!(redo ? redo_pop(es->undo, &command) : undo_pop(es->undo, &command)))
        return;
    /*
     * One step may be a group of entries (a multi-entity move, delete or
     * paste; see undo.h).  Keep popping while the next entry belongs to the
     * same group.  Undo meets the entries newest first and redo oldest
     * first, so array positions shift back exactly as they shifted forward.
     */
    group = command.group;
    editor_apply_undo_command(es, &command, redo ? 0 : 1);
    if (redo) note_redone_place(placed, &placed_count, &command);
    while (group != 0 &&
           (redo ? redo_top_group(es->undo) : undo_top_group(es->undo)) == group &&
           (redo ? redo_pop(es->undo, &command) : undo_pop(es->undo, &command))) {
        editor_apply_undo_command(es, &command, redo ? 0 : 1);
        if (redo) note_redone_place(placed, &placed_count, &command);
    }
    /* A redone group paste or duplicate selects every copy again. */
    if (placed_count > 1) (void)editor_select_items(es, placed, placed_count);
    editor_refresh_dirty(es);
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
        /* Caret keys act at once, in the order they were pressed, so
         * "type, Left, type" in one frame lands where it should. */
        switch (key) {
        case KEY_BACKSPACE: ui_edit_key(&es->ui, UI_KEY_BACKSPACE); break;
        case KEY_DELETE:    ui_edit_key(&es->ui, UI_KEY_DELETE);    break;
        case KEY_LEFT:      ui_edit_key(&es->ui, UI_KEY_LEFT);      break;
        case KEY_RIGHT:     ui_edit_key(&es->ui, UI_KEY_RIGHT);     break;
        case KEY_HOME:      ui_edit_key(&es->ui, UI_KEY_HOME);      break;
        case KEY_END:       ui_edit_key(&es->ui, UI_KEY_END);       break;
        case KEY_TAB:       ui_focus_next(&es->ui, shift ? -1 : 1); break;
        case KEY_ENTER: case KEY_KP_ENTER: es->ui.key_return = 1;  break;
        default: break;
        }
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
    /* The Campaign view covers the canvas: a level shortcut there would
     * change a document the designer cannot see.  It takes Esc (cancel a
     * name being typed, else close), Ctrl+S (save the campaign) and Ctrl+M
     * (close) only. */
    if (es->campaign) {
        if (key == KEY_ESCAPE && es->ui.active_id)
            ui_cancel_active_edit(&es->ui);
        else if (key == KEY_ESCAPE || (ctrl && key == KEY_M))
            (void)editor_campaign_close(es, 0);
        else if (ctrl && key == KEY_S)
            (void)editor_campaign_save(es);
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
            es->panel_scroll = 0;
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
    case KEY_S:
        if (editor_finish_field_edit(es)) {
            es->snap_to_grid ^= 1;
            editor_set_status(es, es->snap_to_grid
                ? "Snap to grid on: placing and dragging use the 48 px grid (Shift: free)"
                : "Snap to grid off (hold Shift to snap)");
        }
        break;
    case KEY_DELETE:
    case KEY_BACKSPACE:
        /* Backspace too: many laptop keyboards (Macs) have no Delete key.
         * Inside a text field it edits text instead (handled above). */
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
 *   Ctrl+wheel   : step through the zoom presets (1x, 2x, 3x, 5x, stopping
 *                  at either end), keeping the point under the cursor in
 *                  place
 */
static void editor_canvas_wheel(EditorState *es, const InputEvent *event)
{
    /* One wheel notch pans 48 canvas pixels; divide by zoom for world px. */
    float zoom = es->camera.zoom > 0.0f ? es->camera.zoom : 1.0f;
    float step = event->wheel * 48.0f / zoom;

    if (event->mods & INPUT_CTRL) {
        /* These whole-number presets are assigned by startup, the
         * toolbar and this wheel handler; zoom itself is never a fraction,
         * so the current preset is found by comparing integers. */
        static const int zooms[] = {1, 2, 3, 5};
        const int last = (int)(sizeof(zooms) / sizeof(zooms[0])) - 1;
        int index = 1;
        for (int i = 0; i <= last; i++)
            if ((int)es->camera.zoom == zooms[i]) {
                index = i;
                break;
            }
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
         * not suddenly jump back to 1x. */
        if (index < 0) index = 0;
        if (index > last) index = last;
        if ((float)zooms[index] != es->camera.zoom)
            canvas_set_zoom(es, (float)zooms[index], event->x, event->y);
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
    case INPUT_TEXT:
        ui_queue_text_input(&es->ui, event->text);
        break;
    case INPUT_MOUSE_DOWN:
        if (event->button == MOUSE_BUTTON_LEFT) {
            /* An open dropdown list hangs over the canvas and the panels.
             * The press belongs to the list (pick an option or close it);
             * placing or deleting under it as well would be a surprise. */
            if (ui_press(&es->ui)) break;
            es->mouse_down = 1;
            /* Over the Campaign view the canvas tools are asleep; its
             * widgets read the click from ui.mouse_clicked. */
            if (canvas_contains(event->x, event->y) && !es->campaign &&
                editor_finish_field_edit(es))
                tools_mouse_down(es, wx, wy);
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
