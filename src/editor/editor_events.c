/*
 * editor_events.c — SDL event dispatch for the Super Mango editor.
 *
 * Keeps the core editor loop small by routing SDL events to file helpers,
 * undo/redo, clipboard operations, tool actions, panel scrolling, play-test
 * launch, and camera controls.
 */

#include <SDL.h>      /* SDL_Event, SDL_Keycode, SDL_GetMouseState */
#include <string.h>   /* strncpy */

#include "editor_events.h"

#include "canvas.h"           /* canvas_contains */
#include "editor_clipboard.h" /* editor_copy_selected/paste_clipboard */
#include "editor_files.h"     /* save/load/export/autosave helpers */
#include "editor_panels.h"    /* editor_handle_side_panel_scroll */
#include "editor_playtest.h"  /* editor_play_test */
#include "editor_session.h"   /* editor_reset_new_level/set_status */
#include "editor_undo_apply.h"/* editor_apply_undo_command */
#include "tools.h"            /* tools_mouse_* helpers */
#include "undo.h"             /* Command, undo_pop, redo_pop */

/*
 * editor_handle_event — Dispatch a single SDL event to the appropriate handler.
 *
 * Called once per event during the polling loop.  Routes keyboard, mouse,
 * and window events to tool actions, file operations, and UI state updates.
 *
 * Keyboard shortcuts follow common conventions:
 *   Ctrl+S  → save     Ctrl+O  → load (stub)   Ctrl+N  → new level
 *   Ctrl+E  → export   Ctrl+Z  → undo          Ctrl+Shift+Z → redo
 *   1/2/3   → tool select/place/delete
 *   G       → toggle grid    Delete → delete selected entity
 *   Escape  → cancel tool or quit
 */
void editor_handle_event(EditorState *es, SDL_Event *event) {
    switch (event->type) {
    /* ---- Window close (the X button or Alt+F4) ---------------------- */
    case SDL_QUIT:
        if (editor_confirm_discard_changes(es, "quit")) {
            es->running = 0;
        }
        break;

    /* ---- Keyboard --------------------------------------------------- */
    case SDL_KEYDOWN: {
        /*
         * SDL stores modifier key state in event->key.keysym.mod.
         * KMOD_CTRL matches both left and right Ctrl keys.
         * We check modifiers first to route Ctrl+<key> combos before
         * falling through to the unmodified key handlers.
         */
        SDL_Keycode key = event->key.keysym.sym;
        int ctrl  = (event->key.keysym.mod & KMOD_CTRL)  != 0;
        int shift = (event->key.keysym.mod & KMOD_SHIFT) != 0;

        if (ctrl) {
            /* ---- Ctrl+key shortcuts (file I/O, undo/redo) ----------- */
            switch (key) {
            case SDLK_s: {
                /*
                 * Ctrl+S — Save the current level to disk as TOML.
                 *
                 * If no file path has been set yet (first save), default
                 * to "levels/untitled.toml".  After a successful save,
                 * clear the modified flag and update the window title to
                 * reflect the saved state.
                 */
                (void)editor_save_current_level(es);
                break;
            }

            case SDLK_o:
                /*
                 * Ctrl+O — Open a level file via the native OS file picker.
                 *
                 * Shows the platform's file dialog (macOS: NSOpenPanel via
                 * osascript, Linux: zenity, Windows: PowerShell OpenFileDialog).
                 * If the user selects a .toml file, load it into the editor.
                 */
                if (editor_confirm_discard_changes(es, "open another level")) {
                    editor_open_level_file(es);
                }
                break;

            case SDLK_n:
                /*
                 * Ctrl+N — New level.
                 *
                 * Reset the LevelDef to all zeros (empty level), set a
                 * default name, clear the file path and undo history,
                 * and reset the modified flag.  This gives the designer
                 * a clean slate without restarting the editor.
                 *
                 * memset zeroes every byte: all counts become 0, all
                 * positions become 0.0f, and all pointers become NULL.
                 */
                if (editor_confirm_discard_changes(es, "create a new level")) {
                    editor_reset_new_level(es);
                }
                break;

            case SDLK_e:
                /*
                 * Ctrl+E — Export the level as compilable C source files.
                 *
                 * Derives the variable name from the file path by stripping
                 * the directory and extension.  For example:
                 *   "levels/level_02.toml" → "level_02"
                 *
                 * If no file has been saved yet, uses "untitled" as the
                 * variable name.  The export writes two files:
                 *   src/levels/<var_name>.h  — extern declaration
                 *   src/levels/<var_name>.c  — full const LevelDef initialiser
                 */
                (void)editor_export_current_level(es);
                break;

            case SDLK_r:
                if (editor_file_exists(es->autosave_path)) {
                    if (editor_confirm_discard_changes(es, "recover autosave")) {
                        if (editor_load_level(es, es->autosave_path) == 0) {
                            editor_set_status(es, "Recovered autosave");
                        } else {
                            editor_set_status(es, "Failed to recover autosave");
                        }
                    }
                } else {
                    editor_set_status(es, "No autosave to recover");
                }
                break;

            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
            case SDLK_5: {
                int recent_idx = (int)(key - SDLK_1);
                if (recent_idx >= 0 && recent_idx < es->recent_file_count) {
                    if (editor_confirm_discard_changes(es, "open a recent level")) {
                        (void)editor_load_level(es, es->recent_files[recent_idx]);
                    }
                }
                break;
            }

            case SDLK_z:
                if (shift) {
                    /*
                     * Ctrl+Shift+Z — Redo.
                     *
                     * Pop the most recently undone command from the redo stack
                     * and re-apply its "after" state to the level.
                     * reverse=0 means "apply forward" (use the after snapshot).
                     */
                    Command cmd;
                    if (redo_pop(es->undo, &cmd)) {
                        editor_apply_undo_command(es, &cmd, 0);
                        es->modified = 1;
                    }
                } else {
                    /*
                     * Ctrl+Z — Undo.
                     *
                     * Pop the most recent command from the undo stack and
                     * reverse it (apply the "before" state to the level).
                     * reverse=1 means "apply backward" (use the before snapshot).
                     */
                    Command cmd;
                    if (undo_pop(es->undo, &cmd)) {
                        editor_apply_undo_command(es, &cmd, 1);
                        es->modified = 1;
                    }
                }
                break;

            case SDLK_c:
                /*
                 * Ctrl+C — Copy the selected entity to the clipboard.
                 *
                 * Snapshots the entity's placement data so Ctrl+V can
                 * create a duplicate at a nearby position.
                 */
                editor_copy_selected(es);
                break;

            case SDLK_v:
                /*
                 * Ctrl+V — Paste the clipboard entity as a new copy.
                 *
                 * Creates a duplicate of the last-copied entity, offset
                 * 24px right and 24px down so it doesn't overlap the
                 * original.  The new entity is auto-selected for
                 * immediate repositioning.
                 */
                editor_paste_clipboard(es);
                break;

            default:
                break;
            }
        } else {
            /* ---- Unmodified key shortcuts ----------------------------- */
            switch (key) {
            case SDLK_ESCAPE:
                /*
                 * Escape cycles through cancel actions:
                 *   1. If placing or deleting, switch back to select tool.
                 *   2. If an entity is selected, deselect it (shows level config).
                 *   3. Otherwise, do nothing (editor stays open).
                 */
                if (es->tool == TOOL_PLACE || es->tool == TOOL_DELETE) {
                    es->tool = TOOL_SELECT;
                } else if (es->selection.index >= 0) {
                    es->selection.index = -1;
                    es->panel_scroll = 0;
                }
                break;

            case SDLK_F5:
                /*
                 * F5 — Play-test: export the level and launch the game.
                 *
                 * Exports the current level as C source, compiles
                 * and runs the game in a background process.  The editor
                 * stays open so the designer can keep editing while testing.
                 */
                editor_play_test(es);
                break;

            case SDLK_g:
                /*
                 * G — Toggle the grid overlay on the canvas.
                 *
                 * XOR with 1 flips 0↔1.  The canvas_render function checks
                 * show_grid each frame to decide whether to draw grid lines.
                 */
                es->show_grid ^= 1;
                break;

            case SDLK_DELETE:
                /*
                 * Delete — Remove the currently selected entity from the level.
                 *
                 * Only acts when something is selected (index >= 0).
                 * tools_delete_selected records the action on the undo stack
                 * and removes the entity from the appropriate LevelDef array.
                 */
                if (es->selection.index >= 0) {
                    tools_delete_selected(es);
                }
                break;

            /* ---- Tool selection via number keys ---------------------- */
            /*
             * 1/2/3 switch the active tool.  This mirrors the toolbar
             * buttons and provides a fast keyboard-only workflow.
             */
            case SDLK_1:
                es->tool = TOOL_SELECT;
                break;
            case SDLK_2:
                es->tool = TOOL_PLACE;
                break;
            case SDLK_3:
                es->tool = TOOL_DELETE;
                break;

            /* ---- Text input keys forwarded to the UI system ---------- */
            /*
             * Backspace and Return are not delivered via SDL_TEXTINPUT
             * (that event only fires for printable characters).  We set
             * flags in the UIState so that active input fields can detect
             * these keys during their widget logic.
             */
            case SDLK_BACKSPACE:
                es->ui.key_backspace = 1;
                break;
            case SDLK_RETURN:
                es->ui.key_return = 1;
                break;

            default:
                break;
            }
        }
        break;
    }

    /* ---- Text input (printable characters) -------------------------- */
    case SDL_TEXTINPUT:
        /*
         * SDL_TEXTINPUT — fired when the OS delivers a printable character.
         *
         * This event handles keyboard layout mapping and dead-key composition.
         * We copy the text into the UIState so that the active text/number
         * input field can append it to its edit buffer this frame.
         *
         * event->text.text is a UTF-8 string (usually 1 character).
         * We copy up to 31 bytes + NUL to fit the UIState's 32-byte buffer.
         */
        strncpy(es->ui.text_input, event->text.text,
                sizeof(es->ui.text_input) - 1);
        es->ui.text_input[sizeof(es->ui.text_input) - 1] = '\0';
        es->ui.has_text_input = 1;
        break;

    /* ---- Mouse button down ------------------------------------------ */
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            /*
             * Left click — update the mouse_down state and notify the UI
             * system that a click occurred this frame.  Then forward the
             * click to the tool system with world-space coordinates.
             *
             * Screen-to-world conversion:
             *   world_x = screen_x / zoom + camera.x
             *   world_y = (screen_y - TOOLBAR_H) / zoom
             *
             * We subtract TOOLBAR_H because the canvas starts below the
             * toolbar; screen_y = 0 is the top of the toolbar, not the
             * top of the canvas.
             */
            es->mouse_down       = 1;
            es->ui.mouse_clicked = 1;

            /*
             * Only forward clicks to the tool system when the cursor is
             * inside the canvas area.  Clicks on the panel (palette,
             * properties) must not be interpreted as canvas actions —
             * otherwise clicking a property field deselects the entity.
             */
            if (canvas_contains(event->button.x, event->button.y)) {
                float world_x = (float)event->button.x / es->camera.zoom
                                + es->camera.x;
                float world_y = (float)(event->button.y - TOOLBAR_H)
                                / es->camera.zoom;

                tools_mouse_down(es, world_x, world_y);
            }
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            /*
             * Right click — quick-delete shortcut.
             *
             * Regardless of the active tool, right-clicking an entity
             * deletes it.  This is a common editor UX convention that
             * saves switching to the delete tool for one-off removals.
             */
            es->mouse_right_down = 1;

            /* Only process right-clicks that land on the canvas */
            if (canvas_contains(event->button.x, event->button.y)) {
                float world_x = (float)event->button.x / es->camera.zoom
                                + es->camera.x;
                float world_y = (float)(event->button.y - TOOLBAR_H)
                                / es->camera.zoom;

                tools_right_click(es, world_x, world_y);
            }
        }
        break;

    /* ---- Mouse button up -------------------------------------------- */
    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            /*
             * Left release — end any drag operation and notify the tools.
             *
             * tools_mouse_up finalises the action (e.g. commits a move
             * to the undo stack with the entity's new position).
             */
            es->mouse_down = 0;

            float world_x = (float)event->button.x / es->camera.zoom
                            + es->camera.x;
            float world_y = (float)(event->button.y - TOOLBAR_H)
                            / es->camera.zoom;

            tools_mouse_up(es, world_x, world_y);
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            es->mouse_right_down = 0;
        }
        break;

    /* ---- Mouse wheel (scroll / zoom) --------------------------------- */
    case SDL_MOUSEWHEEL: {
        /*
         * Scroll wheel — horizontal camera pan or zoom.
         *
         * Default: scroll wheel pans the camera left/right so the designer
         * can smoothly scroll through the entire level without holding keys.
         *   Scroll up   (y > 0): pan left  (show earlier part of level).
         *   Scroll down (y < 0): pan right (show later part of level).
         *
         * With Ctrl held: cycle the zoom level (1x → 2x → 4x).
         *
         * Only applies when the cursor is over the canvas area.
         */
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        /*
         * Right panel scroll — route to cfg_scroll or palette_scroll based
         * on which section the cursor is over.
         *
         * The config section occupies [TOOLBAR_H, TOOLBAR_H + config_h).
         * Everything below it (palette + properties) scrolls the palette.
         * We recompute config_h here using the same logic as the render
         * path so the hit-test matches what the user sees.
         */
        if (editor_handle_side_panel_scroll(es, mx, my, event->wheel.y)) {
            break;
        }

        /* Hit test: cursor must be inside the canvas rectangle */
        if (mx < CANVAS_W && my > TOOLBAR_H && my < EDITOR_H - STATUS_H) {
            int ctrl_held = (SDL_GetModState() & KMOD_CTRL) != 0;

            if (ctrl_held) {
                /* Ctrl + scroll → cycle zoom: 1x → 2x → 3x → 5x */
                static const float zooms[] = { 1.0f, 2.0f, 3.0f, 5.0f };
                static const int zoom_count = 4;
                int cur = 1; /* default to 2x index */
                for (int zi = 0; zi < zoom_count; zi++) {
                    if (es->camera.zoom == zooms[zi]) { cur = zi; break; }
                }
                if (event->wheel.y > 0)
                    cur = (cur + 1) % zoom_count;
                else if (event->wheel.y < 0)
                    cur = (cur - 1 + zoom_count) % zoom_count;
                es->camera.zoom = zooms[cur];
            } else {
                /*
                 * Scroll → horizontal pan.
                 *
                 * Move the camera by 48px per scroll step (one TILE_SIZE)
                 * divided by the current zoom so the visual scroll distance
                 * feels consistent at any zoom level.
                 *
                 * Clamp to world boundaries: camera.x stays between 0 and
                 * the maximum scroll position (WORLD_W minus the visible
                 * canvas width in world coordinates).
                 */
                float scroll_step = 48.0f / es->camera.zoom;
                es->camera.x -= event->wheel.y * scroll_step;

                /* Clamp camera to world boundaries */
                int eww = (es->level.screen_count > 0 ? es->level.screen_count : 4) * GAME_W;
                float max_x = (float)eww - (float)CANVAS_W / es->camera.zoom;
                if (es->camera.x < 0.0f) es->camera.x = 0.0f;
                if (es->camera.x > max_x) es->camera.x = max_x;
            }
        }
        break;
    }

    /* ---- Mouse motion (drag) ---------------------------------------- */
    case SDL_MOUSEMOTION:
        /*
         * Mouse move — forward to the tool system if dragging.
         *
         * tools_mouse_drag handles both entity dragging (select tool) and
         * camera panning (middle-click, though not yet implemented).
         * We only call it while the left button is held (mouse_down == 1)
         * to avoid processing every idle cursor movement.
         */
        if (es->mouse_down) {
            float world_x = (float)event->motion.x / es->camera.zoom
                            + es->camera.x;
            float world_y = (float)(event->motion.y - TOOLBAR_H)
                            / es->camera.zoom;

            tools_mouse_drag(es, world_x, world_y);
        }
        break;

    default:
        break;
    }
}
