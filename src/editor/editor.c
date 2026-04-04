/*
 * editor.c — Core implementation of the Super Mango level editor.
 *
 * Implements the three lifecycle functions declared in editor.h:
 *   editor_init    — create window, renderer, font, textures, undo stack.
 *   editor_loop    — main event/render loop at 60 FPS.
 *   editor_cleanup — free every resource in reverse init order.
 *
 * Also contains static helpers that only this file calls:
 *   load_textures      — load all entity sprite sheets from assets/.
 *   handle_event       — dispatch one SDL event to the correct handler.
 *   render_toolbar     — draw the top toolbar (tools, zoom, file buttons).
 *   render_status_bar  — draw the bottom status bar (cursor, tool, file info).
 *   apply_undo_command — apply or reverse an undo command on the level.
 *
 * The editor follows the same single-struct-by-pointer pattern as the game:
 * EditorState holds every resource, and is passed to every function.
 */

#include <SDL.h>          /* SDL_Window, SDL_Renderer, SDL_Event, etc.     */
#include <SDL_image.h>    /* IMG_LoadTexture — load PNG sprite sheets      */
#include <SDL_ttf.h>      /* TTF_OpenFont, TTF_CloseFont — text rendering  */
#include <stdio.h>        /* fprintf, stderr, snprintf — error and status  */
#include <string.h>       /* memset, strncpy, strrchr — string operations  */

#ifndef _WIN32
#include <unistd.h>       /* fork, execl, _exit — POSIX process control    */
#include <signal.h>       /* kill — send signal to child process            */
#include <sys/wait.h>     /* waitpid, WNOHANG — non-blocking child check   */
#endif

#include "editor.h"       /* EditorState, constants, EntityType, EditorTool */
#include "canvas.h"       /* canvas_render — draw the level preview         */
#include "palette.h"      /* palette_render — draw the entity palette panel */
#include "properties.h"   /* properties_render — draw the selection inspector */
#include "tools.h"        /* tools_mouse_down/up/drag/right_click/delete    */
#include "undo.h"         /* UndoStack, undo_create/destroy/push/pop/clear  */
#include "serializer.h"   /* level_save_json, level_load_json               */
#include "exporter.h"     /* level_export_c — write .h/.c from LevelDef    */
#include "ui.h"           /* UIState, ui_init, ui_begin_frame, ui_button    */
#include "file_dialog.h"  /* file_dialog_open — native OS file picker       */

/* ------------------------------------------------------------------ */
/* Forward declarations for static helpers                             */
/* ------------------------------------------------------------------ */

static void load_textures(EditorState *es);
static void handle_event(EditorState *es, SDL_Event *event);
static void render_toolbar(EditorState *es);
static void render_status_bar(EditorState *es);
static void apply_undo_command(EditorState *es, const Command *cmd, int reverse);
static void open_level_file(EditorState *es);
static void load_level_from_path(EditorState *es, const char *path);
static void copy_selected(EditorState *es);
static void paste_clipboard(EditorState *es);
static void play_test(EditorState *es);
static void stop_play(EditorState *es);
static void check_play_status(EditorState *es);

/* ------------------------------------------------------------------ */
/* editor_init                                                         */
/* ------------------------------------------------------------------ */

/*
 * editor_init — Create the editor window, renderer, and load all resources.
 *
 * Called once at startup from editor_main.c.  Initialises every field in
 * EditorState: SDL window and renderer, TTF font, entity textures, undo
 * stack, camera defaults, and tool state.
 *
 * The editor window is 1280x720 — larger than the game's 800x600 because
 * the editor needs room for the level canvas plus a side panel for the
 * entity palette and property inspector.
 *
 * Returns 0 on success, -1 on fatal error.  Fatal errors print to stderr
 * via SDL_GetError() / TTF_GetError() before returning.
 */
int editor_init(EditorState *es) {
    /*
     * SDL_SetHint — request nearest-neighbour texture scaling.
     *
     * "0" selects point filtering so pixel art stays crisp when rendered at
     * non-native sizes.  Without this, SDL defaults to bilinear filtering
     * which blurs sharp pixel edges.  Must be set BEFORE creating textures.
     */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    /*
     * SDL_CreateWindow — ask the OS for a window.
     *
     * "Super Mango Editor" → title bar text (updated later to show file name).
     * SDL_WINDOWPOS_CENTERED → center on monitor for both x and y.
     * EDITOR_W x EDITOR_H   → 1280x720 pixels (defined in editor.h).
     * SDL_WINDOW_SHOWN       → make the window visible immediately.
     */
    es->window = SDL_CreateWindow(
        "Super Mango Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        EDITOR_W, EDITOR_H,
        SDL_WINDOW_SHOWN
    );
    if (!es->window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return -1;
    }

    /*
     * SDL_CreateRenderer — create a GPU-accelerated 2D drawing context.
     *
     * -1                          → use the first rendering driver that supports
     *                                the requested flags (usually the only GPU).
     * SDL_RENDERER_ACCELERATED    → require hardware acceleration (no software fallback).
     * SDL_RENDERER_PRESENTVSYNC   → synchronise presents with the monitor's refresh
     *                                rate to avoid screen tearing.  This also provides
     *                                a natural frame-rate cap (typically 60 Hz).
     */
    es->renderer = SDL_CreateRenderer(
        es->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!es->renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(es->window);
        es->window = NULL;
        return -1;
    }

    /*
     * TTF_OpenFont — load a TrueType font file for text rendering.
     *
     * "assets/round9x13.ttf" is the same monospaced pixel font the game's
     * debug overlay uses.  Size 13 gives clear, compact text at 1x scale.
     *
     * Font loading is fatal because the editor is unusable without text —
     * every panel label, tooltip, and status message requires the font.
     */
    es->font = TTF_OpenFont("assets/fonts/round9x13.ttf", 13);
    if (!es->font) {
        fprintf(stderr, "TTF_OpenFont error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(es->renderer);
        es->renderer = NULL;
        SDL_DestroyWindow(es->window);
        es->window = NULL;
        return -1;
    }

    /* ---- Camera defaults -------------------------------------------- */
    /*
     * Start with the camera at the left edge of the level (x=0) and a
     * 2x zoom so entities are clearly visible on the 1280-wide canvas.
     * Zoom cycles through 1x → 2x → 4x via the scroll wheel.
     */
    es->camera.x    = 0.0f;
    es->camera.zoom = 2.0f;

    /* ---- Tool and selection defaults -------------------------------- */
    /*
     * Default to the select tool (click to pick existing entities).
     * selection.index = -1 is the "nothing selected" sentinel; every
     * function that reads the selection checks for -1 first.
     * palette_type starts at ENT_COIN — a safe default for placing.
     */
    es->tool            = TOOL_SELECT;
    es->selection.index = -1;
    es->palette_type    = ENT_COIN;

    /* ---- Grid toggle ------------------------------------------------ */
    /*
     * show_grid = 1 draws grid lines on the canvas by default.
     * The user can toggle this with the 'G' key.  Grid lines help align
     * entities to TILE_SIZE boundaries without needing a snap-to-grid mode.
     */
    es->show_grid = 1;

    /* ---- Undo stack ------------------------------------------------- */
    /*
     * undo_create — heap-allocate a zeroed UndoStack.
     *
     * The undo stack is ~24 KB (two 256-entry Command arrays) — too large
     * for the C stack on some platforms, so it lives on the heap.
     * The editor owns this pointer and frees it in editor_cleanup.
     */
    es->undo = undo_create();
    if (!es->undo) {
        fprintf(stderr, "Failed to allocate undo stack\n");
        TTF_CloseFont(es->font);
        es->font = NULL;
        SDL_DestroyRenderer(es->renderer);
        es->renderer = NULL;
        SDL_DestroyWindow(es->window);
        es->window = NULL;
        return -1;
    }

    /* ---- UI state --------------------------------------------------- */
    /*
     * ui_init — store the renderer and font pointers in the UIState.
     *
     * The immediate-mode UI reads these every frame to draw widgets.
     * All other UIState fields start at zero (no active widget, no text
     * input, mouse at origin).
     */
    ui_init(&es->ui, es->renderer, es->font);

    /* ---- Entity textures -------------------------------------------- */
    /*
     * Load every entity sprite sheet from the shared assets/ directory.
     * These are the same PNGs the game engine uses, so the editor shows
     * exactly what will appear in-game (WYSIWYG).
     */
    load_textures(es);

    /* ---- Default level ---------------------------------------------- */
    /*
     * Set a default level name so the title bar and status bar have
     * something to display before the user saves or loads a file.
     */
    strncpy(es->level.name, "Untitled", sizeof(es->level.name) - 1);

    /* ---- Start the loop --------------------------------------------- */
    /*
     * running = 1 keeps the main loop active.  Setting it to 0 (via
     * Escape key, window close, or SDL_QUIT event) exits the loop.
     */
    es->running = 1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* load_textures — static helper                                       */
/* ------------------------------------------------------------------ */

/*
 * load_textures — Load all entity sprite sheets from assets/.
 *
 * Called once from editor_init after the renderer is created.  Each texture
 * is loaded with IMG_LoadTexture which decodes the PNG and uploads it to
 * GPU memory.  The returned SDL_Texture* lives on the GPU until destroyed.
 *
 * All loads are NON-FATAL: if a sprite sheet is missing (e.g. the designer
 * deleted an asset), the editor logs a warning and sets the pointer to NULL.
 * Canvas rendering checks for NULL and draws a placeholder rectangle instead.
 * This lets the editor remain usable even with incomplete asset directories.
 *
 * The macro LOAD_TEX reduces boilerplate: it calls IMG_LoadTexture and
 * prints a warning if the result is NULL.
 */
static void load_textures(EditorState *es) {
    /*
     * LOAD_TEX — attempt to load a texture; warn on failure but continue.
     *
     * field : member of es->textures to assign (e.g. floor_tile).
     * path  : file path relative to the working directory (repo root).
     *
     * IMG_GetError() returns the SDL_image error string for the most recent
     * failure — we include it in the warning so the designer can diagnose
     * missing or corrupted files.
     */
    #define LOAD_TEX(field, path) \
        do { \
            es->textures.field = IMG_LoadTexture(es->renderer, path); \
            if (!es->textures.field) { \
                fprintf(stderr, "Warning: could not load %s: %s\n", \
                        path, IMG_GetError()); \
            } \
        } while (0)

    /* Floor and water — the base environment textures */
    LOAD_TEX(floor_tile,       "assets/sprites/levels/grass_tileset.png");
    LOAD_TEX(water,            "assets/sprites/effects/water.png");

    /* Static geometry */
    LOAD_TEX(platform,         "assets/sprites/surfaces/platform.png");

    /* Enemies — ground, air, and water patrol types */
    LOAD_TEX(spider,           "assets/sprites/entities/spider.png");
    LOAD_TEX(jumping_spider,   "assets/sprites/entities/jumping_spider.png");
    LOAD_TEX(bird,             "assets/sprites/entities/bird.png");
    LOAD_TEX(faster_bird,      "assets/sprites/entities/faster_bird.png");
    LOAD_TEX(fish,             "assets/sprites/entities/fish.png");
    LOAD_TEX(faster_fish,      "assets/sprites/entities/faster_fish.png");

    /* Collectibles — coins, stars */
    LOAD_TEX(coin,             "assets/sprites/collectibles/coin.png");
    LOAD_TEX(yellow_star,      "assets/sprites/collectibles/yellow_star.png");
    LOAD_TEX(last_star,        "assets/sprites/collectibles/last_star.png");

    /* Hazards — traps that damage the player on contact */
    LOAD_TEX(axe_trap,         "assets/sprites/hazards/axe_trap.png");
    LOAD_TEX(circular_saw,     "assets/sprites/hazards/circular_saw.png");
    LOAD_TEX(blue_flame,       "assets/sprites/hazards/blue_flame.png");
    LOAD_TEX(spike,            "assets/sprites/hazards/spike.png");
    LOAD_TEX(spike_platform,   "assets/sprites/hazards/spike_platform.png");
    LOAD_TEX(spike_block,      "assets/sprites/hazards/spike_block.png");

    /* Surfaces — platforms, bridges, bouncepads */
    LOAD_TEX(float_platform,   "assets/sprites/surfaces/float_platform.png");
    LOAD_TEX(bridge,           "assets/sprites/surfaces/bridge.png");
    LOAD_TEX(bouncepad_small,  "assets/sprites/surfaces/bouncepad_small.png");
    LOAD_TEX(bouncepad_medium, "assets/sprites/surfaces/bouncepad_medium.png");
    LOAD_TEX(bouncepad_high,   "assets/sprites/surfaces/bouncepad_high.png");

    /* Climbables — vertical traversal */
    LOAD_TEX(vine,             "assets/sprites/surfaces/vine.png");
    LOAD_TEX(ladder,           "assets/sprites/surfaces/ladder.png");
    LOAD_TEX(rope,             "assets/sprites/surfaces/rope.png");

    /* Rail paths — spike blocks and platforms ride on these */
    LOAD_TEX(rail,             "assets/sprites/surfaces/rail.png");

    #undef LOAD_TEX
}

/* ------------------------------------------------------------------ */
/* editor_loop                                                         */
/* ------------------------------------------------------------------ */

/*
 * editor_loop — Run the editor's main event/render loop until exit.
 *
 * Each frame:
 *   1. Reset per-frame UI input state (clicks, key presses, text input).
 *   2. Poll and dispatch all queued SDL events via handle_event().
 *   3. Update mouse position for the immediate-mode UI.
 *   4. Clear the screen to a dark grey background.
 *   5. Render the level canvas (entities, grid, floor).
 *   6. Render the toolbar, palette panel, property inspector, status bar.
 *   7. Present the back buffer to the screen.
 *   8. Cap the frame rate to ~60 FPS if VSync did not already do so.
 *
 * The loop exits when es->running is set to 0 (by handle_event on
 * SDL_QUIT or the Escape key).
 */
void editor_loop(EditorState *es) {
    SDL_Event event;

    while (es->running) {
        /*
         * SDL_GetTicks — milliseconds since SDL_Init.
         *
         * We record the start time so we can compute elapsed time at the
         * end of the frame and sleep if we finished early.  This provides
         * a manual frame-rate cap as a fallback in case VSync is not
         * supported by the driver.
         */
        Uint32 frame_start = SDL_GetTicks();

        /* ---- Reset per-frame input ---------------------------------- */
        /*
         * ui_begin_frame clears the one-shot input flags (mouse_clicked,
         * key_backspace, key_return, key_escape, text_input) so that only
         * events from THIS frame are seen by widget functions.  Without
         * this reset a single click would register on multiple frames.
         */
        ui_begin_frame(&es->ui);
        es->ui.mouse_clicked  = 0;
        es->ui.key_backspace  = 0;
        es->ui.key_return     = 0;
        es->ui.key_escape     = 0;
        es->ui.has_text_input = 0;

        /* ---- Event polling ------------------------------------------ */
        /*
         * SDL_PollEvent — dequeue one event from SDL's internal queue.
         *
         * Returns 1 if an event was available (and fills `event`), or 0
         * when the queue is empty.  We loop until the queue is drained
         * so that every input within a single frame is processed before
         * rendering.  This prevents input lag from accumulating.
         */
        while (SDL_PollEvent(&event)) {
            handle_event(es, &event);
        }

        /* ---- Update mouse state for the UI -------------------------- */
        /*
         * SDL_GetMouseState — query the current mouse position.
         *
         * Updates es->mouse_x/y with the cursor's window-pixel coordinates.
         * We also copy them into the UIState so that immediate-mode widgets
         * (buttons, dropdowns, input fields) can do hit testing this frame.
         */
        SDL_GetMouseState(&es->mouse_x, &es->mouse_y);
        es->ui.mouse_x = es->mouse_x;
        es->ui.mouse_y = es->mouse_y;

        /* ---- Check if game child process exited --------------------- */
        /*
         * When the editor is in play-test mode, check each frame whether
         * the game process has exited.  If it has, return to editor mode.
         */
        if (es->playing) {
            check_play_status(es);
        }

        /* ---- Clear the screen --------------------------------------- */
        SDL_SetRenderDrawColor(es->renderer, 0x1A, 0x1A, 0x1A, 0xFF);
        SDL_RenderClear(es->renderer);

        if (es->playing) {
            /* ---- Play-test mode: show minimal overlay --------------- */
            /*
             * While the game is running, the editor shows a dark screen
             * with a centered "Playing..." message and a [Stop] button.
             * This makes it clear the game is active and provides a way
             * to terminate it without switching windows.
             */
            ui_label(&es->ui, EDITOR_W / 2 - 60, EDITOR_H / 2 - 40,
                     "Playing level...");
            ui_label_color(&es->ui, EDITOR_W / 2 - 80, EDITOR_H / 2 - 10,
                           "Close the game window or click Stop",
                           UI_TEXT_DIM);

            if (ui_button(&es->ui, EDITOR_W / 2 - 40, EDITOR_H / 2 + 30,
                          80, 28, "Stop")) {
                stop_play(es);
            }
        } else {
            /* ---- Normal editor rendering ----------------------------- */
            canvas_render(es);
            render_toolbar(es);
            palette_render(es);

            if (es->selection.index >= 0) {
                properties_render(es);
            } else {
                /* No entity selected — show level-wide config panel */
                level_config_render(es);
            }

            render_status_bar(es);
        }

        /* ---- Present the frame -------------------------------------- */
        /*
         * SDL_RenderPresent — swap the back buffer to the screen.
         *
         * Everything drawn since SDL_RenderClear becomes visible at once.
         * If VSync is active this call blocks until the next monitor refresh
         * (typically 16.67 ms at 60 Hz), which naturally caps the frame rate.
         */
        SDL_RenderPresent(es->renderer);

        /* ---- Manual frame-rate cap ---------------------------------- */
        /*
         * If the frame completed in less than 16 ms (~60 FPS), sleep for
         * the remainder.  This is a fallback for systems where VSync is
         * unavailable or disabled — without it the editor would spin the
         * CPU at 100% and waste power.
         *
         * SDL_Delay yields the thread to the OS scheduler.  The actual
         * sleep may be slightly longer than requested, but for an editor
         * tool (not a twitchy game) this imprecision is fine.
         */
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < 16) {
            SDL_Delay(16 - elapsed);
        }
    }
}

/* ------------------------------------------------------------------ */
/* handle_event — static helper                                        */
/* ------------------------------------------------------------------ */

/*
 * handle_event — Dispatch a single SDL event to the appropriate handler.
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
static void handle_event(EditorState *es, SDL_Event *event) {
    switch (event->type) {
    /* ---- Window close (the X button or Alt+F4) ---------------------- */
    case SDL_QUIT:
        es->running = 0;
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
                 * Ctrl+S — Save the current level to disk as JSON.
                 *
                 * If no file path has been set yet (first save), default
                 * to "levels/untitled.json".  After a successful save,
                 * clear the modified flag and update the window title to
                 * reflect the saved state.
                 */
                if (es->file_path[0] == '\0') {
                    strncpy(es->file_path, "levels/untitled.json",
                            sizeof(es->file_path) - 1);
                    es->file_path[sizeof(es->file_path) - 1] = '\0';
                }
                if (level_save_json(&es->level, es->file_path) == 0) {
                    es->modified = 0;
                    /*
                     * SDL_SetWindowTitle — update the title bar text.
                     *
                     * Show the file path so the designer always knows
                     * which file is open.  The asterisk (*) that was
                     * appended while modified is gone now.
                     */
                    char title[300];
                    snprintf(title, sizeof(title), "Super Mango Editor - %s",
                             es->file_path);
                    SDL_SetWindowTitle(es->window, title);
                } else {
                    fprintf(stderr, "Error: failed to save %s\n",
                            es->file_path);
                }
                break;
            }

            case SDLK_o:
                /*
                 * Ctrl+O — Open a level file via the native OS file picker.
                 *
                 * Shows the platform's file dialog (macOS: NSOpenPanel via
                 * osascript, Linux: zenity, Windows: PowerShell OpenFileDialog).
                 * If the user selects a .json file, load it into the editor.
                 */
                open_level_file(es);
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
                memset(&es->level, 0, sizeof(es->level));
                strncpy(es->level.name, "Untitled", sizeof(es->level.name) - 1);
                es->file_path[0] = '\0';
                undo_clear(es->undo);
                es->modified        = 0;
                es->selection.index = -1;
                SDL_SetWindowTitle(es->window, "Super Mango Editor");
                break;

            case SDLK_e: {
                /*
                 * Ctrl+E — Export the level as compilable C source files.
                 *
                 * Derives the variable name from the file path by stripping
                 * the directory and extension.  For example:
                 *   "levels/level_02.json" → "level_02"
                 *
                 * If no file has been saved yet, uses "untitled" as the
                 * variable name.  The export writes two files:
                 *   src/levels/<var_name>.h  — extern declaration
                 *   src/levels/<var_name>.c  — full const LevelDef initialiser
                 */
                const char *var_name = "untitled";
                char name_buf[128] = {0};

                if (es->file_path[0] != '\0') {
                    /*
                     * strrchr — find the last '/' in the path to isolate
                     * the filename.  If no slash exists, use the whole path.
                     */
                    const char *base = strrchr(es->file_path, '/');
                    base = base ? base + 1 : es->file_path;

                    /*
                     * Copy the filename (without extension) into name_buf.
                     * We find the '.' and truncate there; if no dot, copy all.
                     */
                    strncpy(name_buf, base, sizeof(name_buf) - 1);
                    name_buf[sizeof(name_buf) - 1] = '\0';
                    char *dot = strrchr(name_buf, '.');
                    if (dot) *dot = '\0';

                    var_name = name_buf;
                }

                if (level_export_c(&es->level, var_name, "src/levels/exported") == 0) {
                    fprintf(stderr, "Exported to src/levels/exported/%s.h/.c\n",
                            var_name);
                } else {
                    fprintf(stderr, "Error: export failed for '%s'\n",
                            var_name);
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
                        apply_undo_command(es, &cmd, 0);
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
                        apply_undo_command(es, &cmd, 1);
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
                copy_selected(es);
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
                paste_clipboard(es);
                break;

            default:
                break;
            }
        } else {
            /* ---- Unmodified key shortcuts ----------------------------- */
            switch (key) {
            case SDLK_ESCAPE:
                /*
                 * Escape has two behaviours:
                 *   - If the place tool is active, switch back to select
                 *     (cancel placement mode without quitting).
                 *   - Otherwise, exit the editor.
                 */
                if (es->tool == TOOL_PLACE) {
                    es->tool = TOOL_SELECT;
                } else {
                    es->running = 0;
                }
                break;

            case SDLK_F5:
                /*
                 * F5 — Play-test: export the level and launch the game.
                 *
                 * Exports the current level as sandbox_00 C source, compiles
                 * and runs the game in a background process.  The editor
                 * stays open so the designer can keep editing while testing.
                 */
                play_test(es);
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

            float world_x = (float)event->button.x / es->camera.zoom
                            + es->camera.x;
            float world_y = (float)(event->button.y - TOOLBAR_H)
                            / es->camera.zoom;

            tools_mouse_down(es, world_x, world_y);
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            /*
             * Right click — quick-delete shortcut.
             *
             * Regardless of the active tool, right-clicking an entity
             * deletes it.  This is a common editor UX convention that
             * saves switching to the delete tool for one-off removals.
             */
            es->mouse_right_down = 1;

            float world_x = (float)event->button.x / es->camera.zoom
                            + es->camera.x;
            float world_y = (float)(event->button.y - TOOLBAR_H)
                            / es->camera.zoom;

            tools_right_click(es, world_x, world_y);
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

        /* Hit test: cursor must be inside the canvas rectangle */
        if (mx < CANVAS_W && my > TOOLBAR_H && my < EDITOR_H - STATUS_H) {
            int ctrl_held = (SDL_GetModState() & KMOD_CTRL) != 0;

            if (ctrl_held) {
                /* Ctrl + scroll → zoom */
                if (event->wheel.y > 0) {
                    if (es->camera.zoom < 1.5f)      es->camera.zoom = 2.0f;
                    else if (es->camera.zoom < 3.0f)  es->camera.zoom = 4.0f;
                    else                               es->camera.zoom = 1.0f;
                } else if (event->wheel.y < 0) {
                    if (es->camera.zoom > 3.0f)       es->camera.zoom = 2.0f;
                    else if (es->camera.zoom > 1.5f)  es->camera.zoom = 1.0f;
                    else                               es->camera.zoom = 4.0f;
                }
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
                float max_x = (float)WORLD_W
                            - (float)CANVAS_W / es->camera.zoom;
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

/* ------------------------------------------------------------------ */
/* render_toolbar — static helper                                      */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* open_level_file / load_level_from_path — file import helpers        */
/* ------------------------------------------------------------------ */

/*
 * load_level_from_path — Load a JSON level file into the editor.
 *
 * Replaces the current level data, clears undo history, resets selection,
 * updates the file path and window title.  Called by both open_level_file
 * (from the dialog) and editor_main.c (from argv[1]).
 */
static void load_level_from_path(EditorState *es, const char *path) {
    LevelDef new_level;
    memset(&new_level, 0, sizeof(new_level));

    if (level_load_json(path, &new_level) != 0) {
        fprintf(stderr, "Error: failed to load %s\n", path);
        return;
    }

    /*
     * Replace the current level with the loaded data.
     * Clear undo/redo history because the commands reference the old level
     * state and would corrupt data if applied to the new level.
     */
    es->level = new_level;
    strncpy(es->file_path, path, sizeof(es->file_path) - 1);
    es->file_path[sizeof(es->file_path) - 1] = '\0';
    undo_clear(es->undo);
    es->selection.index = -1;
    es->modified = 0;

    /* Update the title bar to show the loaded file */
    char title[300];
    snprintf(title, sizeof(title), "Super Mango Editor - %s", es->file_path);
    SDL_SetWindowTitle(es->window, title);

    fprintf(stderr, "Loaded %s (%d entities)\n", path,
            es->level.coin_count + es->level.spider_count +
            es->level.platform_count + es->level.rail_count +
            es->level.bird_count + es->level.fish_count);
}

/*
 * open_level_file — Show the native OS file picker and load the selected file.
 *
 * Uses file_dialog_open() which invokes the platform's file dialog:
 *   macOS  → osascript (AppleScript NSOpenPanel)
 *   Linux  → zenity --file-selection
 *   Windows → PowerShell OpenFileDialog
 *
 * If the user cancels the dialog, nothing happens.
 * If the user selects a file, it's loaded into the editor.
 */
static void open_level_file(EditorState *es) {
    char path[256];

    if (file_dialog_open(path, sizeof(path))) {
        load_level_from_path(es, path);
    }
    /* User cancelled — do nothing */
}

/* ------------------------------------------------------------------ */
/* copy_selected / paste_clipboard — clipboard helpers                  */
/* ------------------------------------------------------------------ */

/*
 * copy_selected — Snapshot the currently selected entity into the clipboard.
 *
 * Uses the same snapshot_entity function from tools.c (via direct struct
 * copy) to capture the placement data.  The clipboard stores the entity
 * type and a PlacementData union so paste can recreate it.
 */
static void copy_selected(EditorState *es) {
    if (es->selection.index < 0) return;

    EntityType t = es->selection.type;
    int i = es->selection.index;

    es->clipboard_type = t;
    es->has_clipboard = 1;

    /* Snapshot the placement data based on entity type */
    switch (t) {
    case ENT_SEA_GAP:
        es->clipboard_data.sea_gap = es->level.sea_gaps[i];
        break;
    case ENT_RAIL:
        es->clipboard_data.rail = es->level.rails[i];
        break;
    case ENT_PLATFORM:
        es->clipboard_data.platform = es->level.platforms[i];
        break;
    case ENT_COIN:
        es->clipboard_data.coin = es->level.coins[i];
        break;
    case ENT_YELLOW_STAR:
        es->clipboard_data.yellow_star = es->level.yellow_stars[i];
        break;
    case ENT_LAST_STAR:
        es->clipboard_data.last_star = es->level.last_star;
        break;
    case ENT_SPIDER:
        es->clipboard_data.spider = es->level.spiders[i];
        break;
    case ENT_JUMPING_SPIDER:
        es->clipboard_data.jumping_spider = es->level.jumping_spiders[i];
        break;
    case ENT_BIRD:
        es->clipboard_data.bird = es->level.birds[i];
        break;
    case ENT_FASTER_BIRD:
        es->clipboard_data.bird = es->level.faster_birds[i];
        break;
    case ENT_FISH:
        es->clipboard_data.fish = es->level.fish[i];
        break;
    case ENT_FASTER_FISH:
        es->clipboard_data.fish = es->level.faster_fish[i];
        break;
    case ENT_AXE_TRAP:
        es->clipboard_data.axe_trap = es->level.axe_traps[i];
        break;
    case ENT_CIRCULAR_SAW:
        es->clipboard_data.circular_saw = es->level.circular_saws[i];
        break;
    case ENT_SPIKE_ROW:
        es->clipboard_data.spike_row = es->level.spike_rows[i];
        break;
    case ENT_SPIKE_PLATFORM:
        es->clipboard_data.spike_platform = es->level.spike_platforms[i];
        break;
    case ENT_SPIKE_BLOCK:
        es->clipboard_data.spike_block = es->level.spike_blocks[i];
        break;
    case ENT_FLOAT_PLATFORM:
        es->clipboard_data.float_platform = es->level.float_platforms[i];
        break;
    case ENT_BRIDGE:
        es->clipboard_data.bridge = es->level.bridges[i];
        break;
    case ENT_BOUNCEPAD_SMALL:
        es->clipboard_data.bouncepad = es->level.bouncepads_small[i];
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        es->clipboard_data.bouncepad = es->level.bouncepads_medium[i];
        break;
    case ENT_BOUNCEPAD_HIGH:
        es->clipboard_data.bouncepad = es->level.bouncepads_high[i];
        break;
    case ENT_VINE:
        es->clipboard_data.vine = es->level.vines[i];
        break;
    case ENT_LADDER:
        es->clipboard_data.ladder = es->level.ladders[i];
        break;
    case ENT_ROPE:
        es->clipboard_data.rope = es->level.ropes[i];
        break;
    default:
        es->has_clipboard = 0;
        break;
    }
}

/*
 * paste_clipboard — Create a new entity from the clipboard data.
 *
 * Inserts a copy of the last Ctrl+C'd entity into the level, offset by
 * 24px right and 24px down so it doesn't overlap the original.  The new
 * entity is auto-selected for immediate repositioning.
 *
 * Respects MAX_* limits — if the array is full, the paste is silently
 * ignored (same behaviour as the place tool at capacity).
 */
static void paste_clipboard(EditorState *es) {
    if (!es->has_clipboard) return;

    EntityType t = es->clipboard_type;
    PlacementData d = es->clipboard_data;

    /*
     * Offset the pasted entity 24px right and 24px down.
     * This nudge makes the paste visible — without it, the copy would
     * sit exactly on top of the original and look like nothing happened.
     */
#define PASTE_OFFSET 24.0f

    /* Helper: offset the x field (and y if present) in the placement data,
     * check capacity, append to array, push undo, select the new entity. */
#define PASTE_INTO(array, count_field, max, data_field)                    \
    do {                                                                   \
        if (es->level.count_field >= (max)) break;                         \
        int idx = es->level.count_field;                                   \
        es->level.array[idx] = d.data_field;                               \
        es->level.count_field++;                                           \
        es->selection.type = t;                                            \
        es->selection.index = idx;                                         \
        es->modified = 1;                                                  \
        /* Push undo command */                                            \
        Command cmd;                                                       \
        memset(&cmd, 0, sizeof(cmd));                                      \
        cmd.type = CMD_PLACE;                                              \
        cmd.entity_type = (int)t;                                          \
        cmd.entity_index = idx;                                            \
        cmd.after.data_field = es->level.array[idx];                       \
        undo_push(es->undo, cmd);                                          \
    } while (0)

    switch (t) {
    case ENT_COIN:
        d.coin.x += PASTE_OFFSET;
        d.coin.y += PASTE_OFFSET;
        PASTE_INTO(coins, coin_count, MAX_COINS, coin);
        break;
    case ENT_YELLOW_STAR:
        d.yellow_star.x += PASTE_OFFSET;
        d.yellow_star.y += PASTE_OFFSET;
        PASTE_INTO(yellow_stars, yellow_star_count, MAX_YELLOW_STARS, yellow_star);
        break;
    case ENT_LAST_STAR:
        d.last_star.x += PASTE_OFFSET;
        d.last_star.y += PASTE_OFFSET;
        es->level.last_star = d.last_star;
        es->modified = 1;
        break;
    case ENT_SPIDER:
        d.spider.x += PASTE_OFFSET;
        d.spider.patrol_x0 += PASTE_OFFSET;
        d.spider.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(spiders, spider_count, MAX_SPIDERS, spider);
        break;
    case ENT_JUMPING_SPIDER:
        d.jumping_spider.x += PASTE_OFFSET;
        d.jumping_spider.patrol_x0 += PASTE_OFFSET;
        d.jumping_spider.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(jumping_spiders, jumping_spider_count, MAX_JUMPING_SPIDERS, jumping_spider);
        break;
    case ENT_BIRD:
        d.bird.x += PASTE_OFFSET;
        d.bird.patrol_x0 += PASTE_OFFSET;
        d.bird.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(birds, bird_count, MAX_BIRDS, bird);
        break;
    case ENT_FASTER_BIRD:
        d.bird.x += PASTE_OFFSET;
        d.bird.patrol_x0 += PASTE_OFFSET;
        d.bird.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(faster_birds, faster_bird_count, MAX_FASTER_BIRDS, bird);
        break;
    case ENT_FISH:
        d.fish.x += PASTE_OFFSET;
        d.fish.patrol_x0 += PASTE_OFFSET;
        d.fish.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(fish, fish_count, MAX_FISH, fish);
        break;
    case ENT_FASTER_FISH:
        d.fish.x += PASTE_OFFSET;
        d.fish.patrol_x0 += PASTE_OFFSET;
        d.fish.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(faster_fish, faster_fish_count, MAX_FASTER_FISH, fish);
        break;
    case ENT_AXE_TRAP:
        d.axe_trap.pillar_x += PASTE_OFFSET;
        PASTE_INTO(axe_traps, axe_trap_count, MAX_AXE_TRAPS, axe_trap);
        break;
    case ENT_CIRCULAR_SAW:
        d.circular_saw.x += PASTE_OFFSET;
        d.circular_saw.patrol_x0 += PASTE_OFFSET;
        d.circular_saw.patrol_x1 += PASTE_OFFSET;
        PASTE_INTO(circular_saws, circular_saw_count, MAX_CIRCULAR_SAWS, circular_saw);
        break;
    case ENT_SPIKE_ROW:
        d.spike_row.x += PASTE_OFFSET;
        PASTE_INTO(spike_rows, spike_row_count, MAX_SPIKE_ROWS, spike_row);
        break;
    case ENT_SPIKE_PLATFORM:
        d.spike_platform.x += PASTE_OFFSET;
        d.spike_platform.y += PASTE_OFFSET;
        PASTE_INTO(spike_platforms, spike_platform_count, MAX_SPIKE_PLATFORMS, spike_platform);
        break;
    case ENT_SPIKE_BLOCK:
        PASTE_INTO(spike_blocks, spike_block_count, MAX_SPIKE_BLOCKS, spike_block);
        break;
    case ENT_FLOAT_PLATFORM:
        d.float_platform.x += PASTE_OFFSET;
        d.float_platform.y += PASTE_OFFSET;
        PASTE_INTO(float_platforms, float_platform_count, MAX_FLOAT_PLATFORMS, float_platform);
        break;
    case ENT_BRIDGE:
        d.bridge.x += PASTE_OFFSET;
        PASTE_INTO(bridges, bridge_count, MAX_BRIDGES, bridge);
        break;
    case ENT_BOUNCEPAD_SMALL:
        d.bouncepad.x += PASTE_OFFSET;
        PASTE_INTO(bouncepads_small, bouncepad_small_count, MAX_BOUNCEPADS_SMALL, bouncepad);
        break;
    case ENT_BOUNCEPAD_MEDIUM:
        d.bouncepad.x += PASTE_OFFSET;
        PASTE_INTO(bouncepads_medium, bouncepad_medium_count, MAX_BOUNCEPADS_MEDIUM, bouncepad);
        break;
    case ENT_BOUNCEPAD_HIGH:
        d.bouncepad.x += PASTE_OFFSET;
        PASTE_INTO(bouncepads_high, bouncepad_high_count, MAX_BOUNCEPADS_HIGH, bouncepad);
        break;
    case ENT_PLATFORM:
        d.platform.x += PASTE_OFFSET;
        PASTE_INTO(platforms, platform_count, MAX_PLATFORMS, platform);
        break;
    case ENT_VINE:
        d.vine.x += PASTE_OFFSET;
        PASTE_INTO(vines, vine_count, MAX_VINES, vine);
        break;
    case ENT_LADDER:
        d.ladder.x += PASTE_OFFSET;
        PASTE_INTO(ladders, ladder_count, MAX_LADDERS, ladder);
        break;
    case ENT_ROPE:
        d.rope.x += PASTE_OFFSET;
        PASTE_INTO(ropes, rope_count, MAX_ROPES, rope);
        break;
    case ENT_SEA_GAP:
        d.sea_gap += (int)PASTE_OFFSET;
        if (es->level.sea_gap_count < MAX_SEA_GAPS) {
            int idx = es->level.sea_gap_count;
            es->level.sea_gaps[idx] = d.sea_gap;
            es->level.sea_gap_count++;
            es->selection.type = t;
            es->selection.index = idx;
            es->modified = 1;
            Command cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.type = CMD_PLACE;
            cmd.entity_type = (int)t;
            cmd.entity_index = idx;
            cmd.after.sea_gap = d.sea_gap;
            undo_push(es->undo, cmd);
        }
        break;
    case ENT_RAIL:
        d.rail.x += (int)PASTE_OFFSET;
        PASTE_INTO(rails, rail_count, MAX_RAILS, rail);
        break;
    default:
        break;
    }

#undef PASTE_INTO
#undef PASTE_OFFSET
}

/* ------------------------------------------------------------------ */
/* play_test / stop_play / check_play_status                           */
/* ------------------------------------------------------------------ */

/*
 * play_test — Export the level, compile the game, and launch it.
 *
 * Workflow:
 *   1. Export the current level as src/levels/sandbox_00.c/.h.
 *   2. Auto-save JSON if a path is set.
 *   3. Compile the game (blocking — we wait for make to finish).
 *   4. Fork the game as a child process.
 *   5. Switch the editor to "playing" mode: the Play button becomes
 *      Stop, and the editor shows a waiting screen.
 *
 * When the user closes the game window (or clicks Stop in the editor),
 * the editor returns to normal editing mode.
 */
static void play_test(EditorState *es) {
    if (es->playing) return;   /* already running */

    /*
     * Step 1 — Save the level as JSON.
     *
     * The game now loads levels/sandbox_00.json at runtime (no compiled
     * .c file needed).  So the Play button just saves the JSON and
     * launches the already-compiled game binary.
     *
     * If the editor has a file path, save there.  Otherwise save to the
     * default sandbox_00.json path so the game can find it.
     */
    const char *save_path = es->file_path[0] != '\0'
                          ? es->file_path
                          : "levels/sandbox_00.json";

    if (level_save_json(&es->level, save_path) != 0) {
        fprintf(stderr, "Play: failed to save %s\n", save_path);
        return;
    }
    es->modified = 0;

    /* Update file path if we used the default */
    if (es->file_path[0] == '\0') {
        strncpy(es->file_path, save_path, sizeof(es->file_path) - 1);
        es->file_path[sizeof(es->file_path) - 1] = '\0';
    }

    {
        char title[300];
        snprintf(title, sizeof(title), "Super Mango Editor - %s",
                 es->file_path);
        SDL_SetWindowTitle(es->window, title);
    }

    /* Step 4 — Launch the game as a child process */
    fprintf(stderr, "Play: launching game...\n");

#ifndef _WIN32
    /*
     * fork() — create a child process that is a copy of the editor.
     *
     * The child calls execl() to replace itself with the game binary.
     * The parent (editor) records the child's PID so it can monitor
     * the game and kill it when the user clicks Stop.
     *
     * execl replaces the child's memory with the game — it never returns
     * on success.  If it fails, _exit(1) terminates the child immediately
     * (using _exit instead of exit avoids running atexit handlers that
     * could corrupt the editor's SDL state in the parent).
     */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process — become the game */
        execl("./out/super-mango", "super-mango", "--sandbox", (char *)NULL);
        /* execl only returns on error */
        _exit(1);
    } else if (pid > 0) {
        /* Parent process — record the child and enter play mode */
        es->play_pid = (int)pid;
        es->playing = 1;
        SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
    } else {
        fprintf(stderr, "Play: fork() failed\n");
    }
#else
    /* Windows: use system() with start to launch non-blocking */
    system("start /B .\\out\\super-mango.exe");
    es->playing = 1;
    SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
#endif
}

/*
 * stop_play — Terminate the game process and return to editor mode.
 *
 * Sends SIGTERM to the game child process (a graceful termination
 * signal that SDL handles by posting an SDL_QUIT event).  Then waits
 * briefly for the child to exit.  If it doesn't exit within a short
 * window, SIGKILL forces termination.
 */
static void stop_play(EditorState *es) {
    if (!es->playing) return;

#ifndef _WIN32
    if (es->play_pid > 0) {
        /*
         * SIGTERM asks the game to quit gracefully.  Most SDL applications
         * handle this by posting SDL_QUIT, which exits the game loop.
         */
        kill((pid_t)es->play_pid, SIGTERM);

        /*
         * waitpid with 0 options blocks until the child exits.
         * This is brief since SIGTERM usually exits the game within
         * one frame (~16ms).
         */
        waitpid((pid_t)es->play_pid, NULL, 0);
        es->play_pid = 0;
    }
#endif

    es->playing = 0;

    /* Restore the editor title bar */
    if (es->file_path[0] != '\0') {
        char title[300];
        snprintf(title, sizeof(title), "Super Mango Editor - %s%s",
                 es->file_path, es->modified ? " *" : "");
        SDL_SetWindowTitle(es->window, title);
    } else {
        SDL_SetWindowTitle(es->window, "Super Mango Editor");
    }
}

/*
 * check_play_status — Non-blocking check if the game process has exited.
 *
 * Called every frame while es->playing == 1.  Uses waitpid with WNOHANG
 * to check without blocking.  If the child has exited (user closed the
 * game window), automatically returns to editor mode.
 */
static void check_play_status(EditorState *es) {
#ifndef _WIN32
    if (es->play_pid > 0) {
        int status;
        /*
         * waitpid with WNOHANG returns immediately:
         *   > 0 : child has exited (returns the child's PID).
         *   0   : child is still running.
         *   -1  : error (child doesn't exist).
         *
         * If the child has exited, we call stop_play to clean up and
         * return to editor mode.
         */
        pid_t result = waitpid((pid_t)es->play_pid, &status, WNOHANG);
        if (result != 0) {
            /* Child exited (either normally or via signal) */
            es->play_pid = 0;
            es->playing = 0;

            /* Restore the editor title */
            if (es->file_path[0] != '\0') {
                char title[300];
                snprintf(title, sizeof(title), "Super Mango Editor - %s%s",
                         es->file_path, es->modified ? " *" : "");
                SDL_SetWindowTitle(es->window, title);
            } else {
                SDL_SetWindowTitle(es->window, "Super Mango Editor");
            }
        }
    }
#else
    (void)es; /* Windows: no PID tracking in this simple implementation */
#endif
}

/* ------------------------------------------------------------------ */
/* render_toolbar — static helper                                      */
/* ------------------------------------------------------------------ */

/*
 * render_toolbar — Draw the top toolbar (32 px tall, full width).
 *
 * Layout from left to right:
 *   [Select] [Place] [Delete]  |  Zoom: 2x  |  [Grid]  |  [New] [Open] [Save] [Export]
 *
 * Tool buttons highlight in the accent colour (blue) when active.
 * File buttons use the default button style.
 */
static void render_toolbar(EditorState *es) {
    /*
     * Draw the toolbar background panel.
     *
     * ui_panel fills a rectangle with the UI_BG colour, providing a
     * visual separation between the toolbar and the canvas below.
     */
    ui_panel(&es->ui, 0, 0, EDITOR_W, TOOLBAR_H);

    /* ---- Tool selection buttons ------------------------------------- */
    /*
     * Each button is 64x24, starting 4px from the left edge and 4px from
     * the top (centred vertically in the 32px toolbar).  Buttons are
     * spaced with a 4px gap between them.
     *
     * When a tool is active we draw it with UI_BTN_ACTIVE colour by
     * temporarily checking if the current tool matches.  ui_button
     * returns 1 on the click frame, so we update es->tool when clicked.
     */
    int bx = 4;
    int by = 4;
    int bw = 64;
    int bh = 24;

    if (ui_button(&es->ui, bx, by, bw, bh, "Select")) {
        es->tool = TOOL_SELECT;
    }
    if (es->tool == TOOL_SELECT) {
        /*
         * Draw a 2px accent-colour underline below the active tool button.
         * This gives a clear visual indicator of which tool is selected.
         */
        SDL_SetRenderDrawColor(es->renderer,
                               UI_BTN_ACTIVE.r, UI_BTN_ACTIVE.g,
                               UI_BTN_ACTIVE.b, UI_BTN_ACTIVE.a);
        SDL_Rect underline = { bx, by + bh, bw, 2 };
        SDL_RenderFillRect(es->renderer, &underline);
    }

    bx += bw + 4;
    if (ui_button(&es->ui, bx, by, bw, bh, "Place")) {
        es->tool = TOOL_PLACE;
    }
    if (es->tool == TOOL_PLACE) {
        SDL_SetRenderDrawColor(es->renderer,
                               UI_BTN_ACTIVE.r, UI_BTN_ACTIVE.g,
                               UI_BTN_ACTIVE.b, UI_BTN_ACTIVE.a);
        SDL_Rect underline = { bx, by + bh, bw, 2 };
        SDL_RenderFillRect(es->renderer, &underline);
    }

    bx += bw + 4;
    if (ui_button(&es->ui, bx, by, bw, bh, "Delete")) {
        es->tool = TOOL_DELETE;
    }
    if (es->tool == TOOL_DELETE) {
        SDL_SetRenderDrawColor(es->renderer,
                               UI_BTN_ACTIVE.r, UI_BTN_ACTIVE.g,
                               UI_BTN_ACTIVE.b, UI_BTN_ACTIVE.a);
        SDL_Rect underline = { bx, by + bh, bw, 2 };
        SDL_RenderFillRect(es->renderer, &underline);
    }

    /* ---- Zoom display ----------------------------------------------- */
    /*
     * Show the current zoom level as "Zoom: Nx" in the middle-left area.
     * This is a read-only label — zoom changes via the scroll wheel.
     */
    bx += bw + 20;
    char zoom_text[32];
    snprintf(zoom_text, sizeof(zoom_text), "Zoom: %dx", (int)es->camera.zoom);
    ui_label(&es->ui, bx, by + 4, zoom_text);

    /* ---- Grid toggle button ----------------------------------------- */
    /*
     * A compact button that shows the current grid state.
     * Clicking toggles show_grid (same as pressing 'G').
     */
    bx += 90;
    const char *grid_label = es->show_grid ? "[Grid]" : " Grid ";
    if (ui_button(&es->ui, bx, by, 56, bh, grid_label)) {
        es->show_grid ^= 1;
    }

    /* ---- File and play buttons (right-aligned) ------------------------ */
    /*
     * Play, Export, Save, Open, New — placed at the right side of the toolbar.
     * We compute positions from the right edge of the window minus the
     * button widths and spacing.
     */
    int rx = EDITOR_W - 4 - 52;   /* rightmost button */
    if (es->playing) {
        /* While the game is running, show Stop instead of Play */
        if (ui_button(&es->ui, rx, by, 52, bh, "Stop")) {
            stop_play(es);
        }
    } else {
        if (ui_button(&es->ui, rx, by, 52, bh, "Play")) {
            play_test(es);
        }
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Export")) {
        /*
         * Duplicate the Ctrl+E export logic inline.
         * Derive variable name from file path, then call level_export_c.
         */
        const char *var_name = "untitled";
        char name_buf[128] = {0};

        if (es->file_path[0] != '\0') {
            const char *base = strrchr(es->file_path, '/');
            base = base ? base + 1 : es->file_path;
            strncpy(name_buf, base, sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            char *dot = strrchr(name_buf, '.');
            if (dot) *dot = '\0';
            var_name = name_buf;
        }

        if (level_export_c(&es->level, var_name, "src/levels/exported") == 0) {
            fprintf(stderr, "Exported to src/levels/exported/%s.h/.c\n", var_name);
        } else {
            fprintf(stderr, "Error: export failed for '%s'\n", var_name);
        }
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Save")) {
        /*
         * Duplicate the Ctrl+S save logic inline.
         * Default to "levels/untitled.json" if no path has been set.
         */
        if (es->file_path[0] == '\0') {
            strncpy(es->file_path, "levels/untitled.json",
                    sizeof(es->file_path) - 1);
            es->file_path[sizeof(es->file_path) - 1] = '\0';
        }
        if (level_save_json(&es->level, es->file_path) == 0) {
            es->modified = 0;
            char title[300];
            snprintf(title, sizeof(title), "Super Mango Editor - %s",
                     es->file_path);
            SDL_SetWindowTitle(es->window, title);
        } else {
            fprintf(stderr, "Error: failed to save %s\n", es->file_path);
        }
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Open")) {
        /*
         * Open button — show the native file dialog and load the selected
         * JSON level file.  Same behaviour as Ctrl+O.
         */
        open_level_file(es);
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "New")) {
        /*
         * Duplicate the Ctrl+N new-level logic inline.
         * Reset level, path, undo stack, and selection.
         */
        memset(&es->level, 0, sizeof(es->level));
        strncpy(es->level.name, "Untitled", sizeof(es->level.name) - 1);
        es->file_path[0] = '\0';
        undo_clear(es->undo);
        es->modified        = 0;
        es->selection.index = -1;
        SDL_SetWindowTitle(es->window, "Super Mango Editor");
    }
}

/* ------------------------------------------------------------------ */
/* render_status_bar — static helper                                   */
/* ------------------------------------------------------------------ */

/*
 * render_status_bar — Draw the bottom status bar (32 px tall, full width).
 *
 * Layout from left to right:
 *   Mouse: (worldX, worldY)  |  Tool: Select  |  Entities: 42  |  path.json *
 *
 * The status bar gives the designer constant feedback about cursor position,
 * active tool, total entity count, and file state (path + modified indicator).
 */
static void render_status_bar(EditorState *es) {
    /*
     * Draw the status bar background panel at the bottom of the window.
     */
    int bar_y = EDITOR_H - STATUS_H;
    ui_panel(&es->ui, 0, bar_y, EDITOR_W, STATUS_H);

    /* ---- Left: mouse world coordinates ------------------------------ */
    /*
     * Convert the current mouse position from screen pixels to world-space
     * logical pixels, then display as "Mouse: (X, Y)".  This helps the
     * designer place entities at precise world coordinates.
     *
     * The same screen-to-world formula used by the tool system:
     *   world_x = screen_x / zoom + camera.x
     *   world_y = (screen_y - TOOLBAR_H) / zoom
     */
    float wx = (float)es->mouse_x / es->camera.zoom + es->camera.x;
    float wy = (float)(es->mouse_y - TOOLBAR_H) / es->camera.zoom;

    char mouse_text[64];
    snprintf(mouse_text, sizeof(mouse_text), "Mouse: (%.0f, %.0f)", wx, wy);
    ui_label(&es->ui, 8, bar_y + 8, mouse_text);

    /* ---- Centre: current tool name ---------------------------------- */
    /*
     * Display the active tool name so the designer always knows what
     * clicking will do, even without looking at the toolbar.
     */
    const char *tool_names[] = { "Select", "Place", "Delete" };
    const char *tool_name = (es->tool >= 0 && es->tool < 3)
                            ? tool_names[es->tool]
                            : "Unknown";

    char tool_text[64];
    snprintf(tool_text, sizeof(tool_text), "Tool: %s", tool_name);
    ui_label(&es->ui, 240, bar_y + 8, tool_text);

    /* ---- Right: entity count and file info --------------------------- */
    /*
     * Sum up all entity counts in the LevelDef to show the total number
     * of placed entities.  This gives a quick sense of level complexity.
     */
    int total = es->level.sea_gap_count
              + es->level.rail_count
              + es->level.platform_count
              + es->level.coin_count
              + es->level.yellow_star_count
              + (es->level.last_star.x != 0.0f || es->level.last_star.y != 0.0f
                 ? 1 : 0)
              + es->level.spider_count
              + es->level.jumping_spider_count
              + es->level.bird_count
              + es->level.faster_bird_count
              + es->level.fish_count
              + es->level.faster_fish_count
              + es->level.axe_trap_count
              + es->level.circular_saw_count
              + es->level.spike_row_count
              + es->level.spike_platform_count
              + es->level.spike_block_count
              + es->level.float_platform_count
              + es->level.bridge_count
              + es->level.bouncepad_small_count
              + es->level.bouncepad_medium_count
              + es->level.bouncepad_high_count
              + es->level.vine_count
              + es->level.ladder_count
              + es->level.rope_count;

    char info_text[128];
    if (es->file_path[0] != '\0') {
        /*
         * Show the file path with a modified indicator.
         * The asterisk (*) after the path tells the designer that unsaved
         * changes exist — a universal convention in text/level editors.
         */
        snprintf(info_text, sizeof(info_text), "Entities: %d  |  %s%s",
                 total, es->file_path, es->modified ? " *" : "");
    } else {
        snprintf(info_text, sizeof(info_text), "Entities: %d  |  (untitled)%s",
                 total, es->modified ? " *" : "");
    }
    ui_label(&es->ui, 450, bar_y + 8, info_text);
}

/* ------------------------------------------------------------------ */
/* apply_undo_command — static helper                                   */
/* ------------------------------------------------------------------ */

/*
 * apply_undo_command — Apply or reverse an undo command on the level.
 *
 * The undo system stores "before" and "after" snapshots for every action.
 * When undoing (reverse=1), we apply the "before" snapshot to restore the
 * previous state.  When redoing (reverse=0), we apply the "after" snapshot.
 *
 * For CMD_PLACE:
 *   Forward (redo): insert the entity at entity_index.
 *   Reverse (undo): remove the entity at entity_index.
 *
 * For CMD_DELETE:
 *   Forward (redo): remove the entity.
 *   Reverse (undo): re-insert the entity.
 *
 * For CMD_MOVE / CMD_PROPERTY:
 *   Forward (redo): overwrite with "after" data.
 *   Reverse (undo): overwrite with "before" data.
 *
 * This function uses a macro (APPLY_ARRAY) to reduce duplication across
 * the 25+ entity types.  Each entity type is a separate fixed-size array
 * in LevelDef, so the insert/remove logic is identical — only the array
 * name, element type, and count variable differ.
 */
static void apply_undo_command(EditorState *es, const Command *cmd,
                               int reverse) {
    /*
     * APPLY_ARRAY — apply an undo command to a specific entity array.
     *
     * arr        : the array in es->level (e.g. es->level.coins).
     * cnt        : the count field (e.g. es->level.coin_count).
     * union_field: the PlacementData union member (e.g. .coin).
     * max_count  : the MAX_* constant for bounds checking.
     *
     * For insertion: shift elements right from entity_index to make room,
     *   then write the snapshot data.  Increment the count.
     * For removal: shift elements left to close the gap.  Decrement count.
     * For overwrite: directly write the snapshot data at entity_index.
     */
    #define APPLY_ARRAY(arr, cnt, union_field, max_count) \
        do { \
            int idx = cmd->entity_index; \
            if (cmd->type == CMD_PLACE) { \
                if (reverse) { \
                    /* Undo a place = remove the entity */ \
                    if (idx >= 0 && idx < cnt) { \
                        for (int i = idx; i < cnt - 1; i++) \
                            arr[i] = arr[i + 1]; \
                        cnt--; \
                    } \
                } else { \
                    /* Redo a place = insert the entity */ \
                    if (cnt < max_count && idx >= 0 && idx <= cnt) { \
                        for (int i = cnt; i > idx; i--) \
                            arr[i] = arr[i - 1]; \
                        arr[idx] = cmd->after.union_field; \
                        cnt++; \
                    } \
                } \
            } else if (cmd->type == CMD_DELETE) { \
                if (reverse) { \
                    /* Undo a delete = re-insert the entity */ \
                    if (cnt < max_count && idx >= 0 && idx <= cnt) { \
                        for (int i = cnt; i > idx; i--) \
                            arr[i] = arr[i - 1]; \
                        arr[idx] = cmd->before.union_field; \
                        cnt++; \
                    } \
                } else { \
                    /* Redo a delete = remove the entity again */ \
                    if (idx >= 0 && idx < cnt) { \
                        for (int i = idx; i < cnt - 1; i++) \
                            arr[i] = arr[i + 1]; \
                        cnt--; \
                    } \
                } \
            } else { \
                /* CMD_MOVE or CMD_PROPERTY: overwrite with before or after */ \
                if (idx >= 0 && idx < cnt) { \
                    arr[idx] = reverse ? cmd->before.union_field \
                                       : cmd->after.union_field; \
                } \
            } \
        } while (0)

    /*
     * Dispatch to the correct LevelDef array based on the entity type.
     *
     * cmd->entity_type is an EntityType enum value that tells us which
     * array in LevelDef this command targets.  The switch covers every
     * placeable entity type defined in editor.h.
     */
    switch (cmd->entity_type) {
    case ENT_COIN:
        APPLY_ARRAY(es->level.coins, es->level.coin_count,
                     coin, MAX_COINS);
        break;

    case ENT_YELLOW_STAR:
        APPLY_ARRAY(es->level.yellow_stars, es->level.yellow_star_count,
                     yellow_star, MAX_YELLOW_STARS);
        break;

    case ENT_LAST_STAR:
        /*
         * LastStar is a single entity (not an array), so undo/redo just
         * overwrites the struct directly.  There is no insert/remove.
         */
        if (reverse) {
            es->level.last_star = cmd->before.last_star;
        } else {
            es->level.last_star = cmd->after.last_star;
        }
        break;

    case ENT_SPIDER:
        APPLY_ARRAY(es->level.spiders, es->level.spider_count,
                     spider, MAX_SPIDERS);
        break;

    case ENT_JUMPING_SPIDER:
        APPLY_ARRAY(es->level.jumping_spiders,
                     es->level.jumping_spider_count,
                     jumping_spider, MAX_JUMPING_SPIDERS);
        break;

    case ENT_BIRD:
        APPLY_ARRAY(es->level.birds, es->level.bird_count,
                     bird, MAX_BIRDS);
        break;

    case ENT_FASTER_BIRD:
        APPLY_ARRAY(es->level.faster_birds, es->level.faster_bird_count,
                     bird, MAX_FASTER_BIRDS);
        break;

    case ENT_FISH:
        APPLY_ARRAY(es->level.fish, es->level.fish_count,
                     fish, MAX_FISH);
        break;

    case ENT_FASTER_FISH:
        APPLY_ARRAY(es->level.faster_fish, es->level.faster_fish_count,
                     fish, MAX_FASTER_FISH);
        break;

    case ENT_AXE_TRAP:
        APPLY_ARRAY(es->level.axe_traps, es->level.axe_trap_count,
                     axe_trap, MAX_AXE_TRAPS);
        break;

    case ENT_CIRCULAR_SAW:
        APPLY_ARRAY(es->level.circular_saws, es->level.circular_saw_count,
                     circular_saw, MAX_CIRCULAR_SAWS);
        break;

    case ENT_SPIKE_ROW:
        APPLY_ARRAY(es->level.spike_rows, es->level.spike_row_count,
                     spike_row, MAX_SPIKE_ROWS);
        break;

    case ENT_SPIKE_PLATFORM:
        APPLY_ARRAY(es->level.spike_platforms,
                     es->level.spike_platform_count,
                     spike_platform, MAX_SPIKE_PLATFORMS);
        break;

    case ENT_SPIKE_BLOCK:
        APPLY_ARRAY(es->level.spike_blocks, es->level.spike_block_count,
                     spike_block, MAX_SPIKE_BLOCKS);
        break;

    case ENT_FLOAT_PLATFORM:
        APPLY_ARRAY(es->level.float_platforms,
                     es->level.float_platform_count,
                     float_platform, MAX_FLOAT_PLATFORMS);
        break;

    case ENT_BRIDGE:
        APPLY_ARRAY(es->level.bridges, es->level.bridge_count,
                     bridge, MAX_BRIDGES);
        break;

    case ENT_BOUNCEPAD_SMALL:
        APPLY_ARRAY(es->level.bouncepads_small,
                     es->level.bouncepad_small_count,
                     bouncepad, MAX_BOUNCEPADS_SMALL);
        break;

    case ENT_BOUNCEPAD_MEDIUM:
        APPLY_ARRAY(es->level.bouncepads_medium,
                     es->level.bouncepad_medium_count,
                     bouncepad, MAX_BOUNCEPADS_MEDIUM);
        break;

    case ENT_BOUNCEPAD_HIGH:
        APPLY_ARRAY(es->level.bouncepads_high,
                     es->level.bouncepad_high_count,
                     bouncepad, MAX_BOUNCEPADS_HIGH);
        break;

    case ENT_PLATFORM:
        APPLY_ARRAY(es->level.platforms, es->level.platform_count,
                     platform, MAX_PLATFORMS);
        break;

    case ENT_VINE:
        APPLY_ARRAY(es->level.vines, es->level.vine_count,
                     vine, MAX_VINES);
        break;

    case ENT_LADDER:
        APPLY_ARRAY(es->level.ladders, es->level.ladder_count,
                     ladder, MAX_LADDERS);
        break;

    case ENT_ROPE:
        APPLY_ARRAY(es->level.ropes, es->level.rope_count,
                     rope, MAX_ROPES);
        break;

    case ENT_RAIL:
        APPLY_ARRAY(es->level.rails, es->level.rail_count,
                     rail, MAX_RAILS);
        break;

    case ENT_SEA_GAP:
        APPLY_ARRAY(es->level.sea_gaps, es->level.sea_gap_count,
                     sea_gap, MAX_SEA_GAPS);
        break;

    default:
        break;
    }

    #undef APPLY_ARRAY
}

/* ------------------------------------------------------------------ */
/* editor_cleanup                                                      */
/* ------------------------------------------------------------------ */

/*
 * editor_cleanup — Release all editor resources in reverse init order.
 *
 * Resources are freed from latest-created to earliest-created, matching
 * the game's safety rule: later resources may depend on earlier ones
 * (textures need the renderer, renderer needs the window).
 *
 * Every pointer is set to NULL after freeing to prevent double-free
 * errors — SDL_DestroyTexture(NULL) and TTF_CloseFont(NULL) are safe
 * no-ops, so a redundant cleanup call will not crash.
 */
void editor_cleanup(EditorState *es) {
    /* ---- Destroy all entity textures -------------------------------- */
    /*
     * DESTROY_TEX — macro from game.h: checks for NULL, calls
     * SDL_DestroyTexture, then sets the pointer to NULL.
     *
     * Textures must be destroyed BEFORE the renderer because they live
     * on the GPU and are associated with the renderer that created them.
     */
    DESTROY_TEX(es->textures.floor_tile);
    DESTROY_TEX(es->textures.water);
    DESTROY_TEX(es->textures.platform);
    DESTROY_TEX(es->textures.spider);
    DESTROY_TEX(es->textures.jumping_spider);
    DESTROY_TEX(es->textures.bird);
    DESTROY_TEX(es->textures.faster_bird);
    DESTROY_TEX(es->textures.fish);
    DESTROY_TEX(es->textures.faster_fish);
    DESTROY_TEX(es->textures.coin);
    DESTROY_TEX(es->textures.yellow_star);
    DESTROY_TEX(es->textures.last_star);
    DESTROY_TEX(es->textures.axe_trap);
    DESTROY_TEX(es->textures.circular_saw);
    DESTROY_TEX(es->textures.blue_flame);
    DESTROY_TEX(es->textures.spike);
    DESTROY_TEX(es->textures.spike_platform);
    DESTROY_TEX(es->textures.spike_block);
    DESTROY_TEX(es->textures.float_platform);
    DESTROY_TEX(es->textures.bridge);
    DESTROY_TEX(es->textures.bouncepad_small);
    DESTROY_TEX(es->textures.bouncepad_medium);
    DESTROY_TEX(es->textures.bouncepad_high);
    DESTROY_TEX(es->textures.vine);
    DESTROY_TEX(es->textures.ladder);
    DESTROY_TEX(es->textures.rope);
    DESTROY_TEX(es->textures.rail);

    /* ---- Font ------------------------------------------------------- */
    /*
     * TTF_CloseFont — release the FreeType font handle.
     * Must happen before TTF_Quit (which tears down FreeType itself).
     */
    if (es->font) {
        TTF_CloseFont(es->font);
        es->font = NULL;
    }

    /* ---- Undo stack ------------------------------------------------- */
    /*
     * undo_destroy — free the heap-allocated UndoStack.
     * Safe to call with NULL (no-op), like SDL destroy functions.
     */
    undo_destroy(es->undo);
    es->undo = NULL;

    /* ---- Renderer --------------------------------------------------- */
    /*
     * SDL_DestroyRenderer — release the GPU drawing context.
     * All textures that were created via this renderer must already be
     * destroyed; calling this with live textures leaks GPU memory.
     */
    if (es->renderer) {
        SDL_DestroyRenderer(es->renderer);
        es->renderer = NULL;
    }

    /* ---- Window ----------------------------------------------------- */
    /*
     * SDL_DestroyWindow — close the OS window.
     * Must be last among SDL resources because the renderer and all
     * GPU state are tied to the window's OpenGL / Metal / D3D context.
     */
    if (es->window) {
        SDL_DestroyWindow(es->window);
        es->window = NULL;
    }
}
