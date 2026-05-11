/*
 * editor.c — Core implementation of the Super Mango level editor.
 *
 * Implements the three lifecycle functions declared in editor.h:
 *   editor_init    — create window, renderer, font, textures, undo stack.
 *   editor_loop    — main event/render loop at 60 FPS.
 *   editor_cleanup — free every resource in reverse init order.
 *
 * Also contains static helpers that only this file calls:
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
#include <errno.h>        /* errno, ECHILD — waitpid error handling         */
#include <unistd.h>       /* fork, execl, _exit — POSIX process control    */
#include <signal.h>       /* kill — send signal to child process            */
#include <sys/wait.h>     /* waitpid, WNOHANG — non-blocking child check   */
#include <sys/stat.h>     /* mkdir, stat — autosave/recent file support     */
#else
#include <direct.h>       /* _mkdir — autosave/recent file support          */
#endif

#include "editor.h"       /* EditorState, constants, EntityType, EditorTool */
#include "canvas.h"       /* canvas_render — draw the level preview         */
#include "palette.h"      /* palette_render — draw the entity palette panel */
#include "properties.h"   /* properties_render — draw the selection inspector */
#include "tools.h"        /* tools_mouse_down/up/drag/right_click/delete    */
#include "undo.h"         /* UndoStack, undo_create/destroy/push/pop/clear  */
#include "serializer.h"   /* level_save_toml, level_load_toml               */
#include "exporter.h"     /* level_export_c — write .h/.c from LevelDef    */
#include "ui.h"           /* UIState, ui_init, ui_begin_frame, ui_button    */
#include "file_dialog.h"  /* file_dialog_open — native OS file picker       */
#include "editor_validation.h" /* editor_validate_level                       */
#include "editor_session.h" /* editor status/title/session helpers           */
#include "editor_textures.h" /* editor_textures_load/cleanup                 */

#define EDITOR_AUTOSAVE_PATH "out/autosave/editor_autosave.toml"
#define EDITOR_RECENT_PATH   "out/editor_recent.txt"
#define EDITOR_RECENT_MAX    5
#define EDITOR_AUTOSAVE_MS   30000u

enum {
    CFG_H_HEADER          = 28,
    CFG_H_MARGIN_TOP      = 8,
    CFG_H_VALIDATION_BASE = 24,
    CFG_H_VALIDATION_ROW  = 18,
    CFG_H_RECENT_HEADER   = 20,
    CFG_H_RECENT_ROW      = 18,
    CFG_H_METADATA        = 72,
    CFG_H_SCREENS         = 24,
    CFG_H_NEXT_PHASE      = 24,
    CFG_H_MUSIC_LABEL     = 24,
    CFG_H_MUSIC_DROPDOWN  = 22,
    CFG_H_MUSIC_VOLUME    = 24,
    CFG_H_FLOOR_TILE      = 30,
    CFG_H_HEARTS_SPACER   = 6,
    CFG_H_HEARTS_ROW      = 22,
    CFG_H_SCORE_ROW       = 24,
    CFG_H_PHYS_HEADER     = 24,
    CFG_H_BG_HEADER       = 24,
    CFG_H_FG_HEADER       = 24,
    CFG_H_FOG_HEADER      = 24,
    CFG_H_MARGIN_BOTTOM   = 10,
    CFG_H_LAYER_ROW       = 20,
    CFG_H_LAYER_ACTIONS   = 24,
    CFG_H_PHYS_ROWS       = 6 * 22
};

/* ------------------------------------------------------------------ */
/* Forward declarations for static helpers                             */
/* ------------------------------------------------------------------ */

static void handle_event(EditorState *es, SDL_Event *event);
static int compute_config_total_height(const EditorState *es);
static void render_toolbar(EditorState *es);
static void render_status_bar(EditorState *es);
static void apply_undo_command(EditorState *es, const Command *cmd, int reverse);
static void open_level_file(EditorState *es);
static int save_current_level(EditorState *es);
static int export_current_level(EditorState *es);
static void maybe_autosave(EditorState *es);
static void load_recent_files(EditorState *es);
static void save_recent_files(const EditorState *es);
static void add_recent_file(EditorState *es, const char *path);
static int file_exists(const char *path);
static void ensure_out_dirs(void);
static void copy_selected(EditorState *es);
static void paste_clipboard(EditorState *es);
static void play_test(EditorState *es);
static void stop_play(EditorState *es);
static void check_play_status(EditorState *es);

/*
 * compute_config_total_height — Measure expanded Level Config content.
 *
 * The render path and mouse-wheel hit-test both need the same height so panel
 * scrolling matches the visible section bounds. Keep every row constant here.
 */
static int compute_config_total_height(const EditorState *es) {
    extern int g_plx_open, g_fg_open, g_fog_open, g_phys_open;
    int validation_h = CFG_H_VALIDATION_BASE
                     + es->validation_report.message_count * CFG_H_VALIDATION_ROW;
    int recent_h = es->recent_file_count > 0
                 ? CFG_H_RECENT_HEADER + es->recent_file_count * CFG_H_RECENT_ROW
                 : 0;
    int total = CFG_H_HEADER + validation_h + recent_h + CFG_H_MARGIN_TOP
              + CFG_H_METADATA + CFG_H_SCREENS + CFG_H_NEXT_PHASE
              + CFG_H_MUSIC_LABEL + CFG_H_MUSIC_DROPDOWN + CFG_H_MUSIC_VOLUME
              + CFG_H_FLOOR_TILE + CFG_H_HEARTS_SPACER + CFG_H_HEARTS_ROW
              + CFG_H_SCORE_ROW + CFG_H_PHYS_HEADER + CFG_H_BG_HEADER
              + CFG_H_FG_HEADER + CFG_H_FOG_HEADER + CFG_H_MARGIN_BOTTOM;

    if (g_plx_open)
        total += es->level.background_layer_count * CFG_H_LAYER_ROW
               + CFG_H_LAYER_ACTIONS;
    if (g_fg_open)
        total += es->level.foreground_layer_count * CFG_H_LAYER_ROW
               + CFG_H_LAYER_ACTIONS;
    if (g_fog_open)
        total += es->level.fog_layer_count * CFG_H_LAYER_ROW
               + CFG_H_LAYER_ACTIONS;
    if (g_phys_open)
        total += CFG_H_PHYS_ROWS;

    return total;
}

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
        /* Dummy/headless drivers may not expose accelerated renderers. */
        es->renderer = SDL_CreateRenderer(es->window, -1, SDL_RENDERER_SOFTWARE);
    }
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
    es->show_grid    = 1;
    es->panel_open   = 1;
    es->config_open  = 1;
    es->palette_open = 1;

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

    strncpy(es->autosave_path, EDITOR_AUTOSAVE_PATH,
            sizeof(es->autosave_path) - 1);
    load_recent_files(es);

    /* ---- Entity textures -------------------------------------------- */
    /*
     * Load every entity sprite sheet from the shared assets/ directory.
     * These are the same PNGs the game engine uses, so the editor shows
     * exactly what will appear in-game (WYSIWYG).
     */
    editor_textures_load(es);

    /* ---- Default level ---------------------------------------------- */
    /*
     * Set a default level name so the title bar and status bar have
     * something to display before the user saves or loads a file.
     */
    editor_level_init_defaults(&es->level);

    /* ---- Start the loop --------------------------------------------- */
    /*
     * running = 1 keeps the main loop active.  Setting it to 0 (via
     * Escape key, window close, or SDL_QUIT event) exits the loop.
     */
    es->running = 1;
    es->last_autosave_ms = SDL_GetTicks();
    if (file_exists(es->autosave_path)) {
        editor_set_status(es, "Autosave found: Ctrl+R to recover");
    } else {
        editor_set_status(es, "Ready");
    }

    return 0;
}

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

        editor_validate_level(&es->level, &es->validation_report);
        maybe_autosave(es);

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

            /* ---- Right panel layout — three stacked sections ---- */
            /*
             * The right column (x = CANVAS_W to EDITOR_W) is divided into
             * up to three vertical sections that share the available space:
             *
             *   1. Level Config — collapsible; always visible at top.
             *   2. Palette      — collapsible; scrollable entity categories.
             *   3. Properties   — only shown when an entity is selected.
             *
             * Each section starts where the previous one ends.  Heights
             * are computed dynamically based on collapse state and whether
             * an entity is selected.
             */
            {
                int right_top    = TOOLBAR_H;
                int right_bottom = EDITOR_H - STATUS_H;
                int section_hdr  = 28;  /* matches palette TITLE_H */

                /* Section 1: Level Config */
                int config_y = right_top;
                int config_h_total; /* uncapped full content height */
                if (es->config_open) {
                    /* Base: header(28) + margin(8) + metadata(72) + screens(24)
                     * + music(24+22+24) + floor(30)
                     * + hearts/lives(6+22) + pts/life(24)
                     * + bg header(24) + fg header(24) + fog header(24)
                     * + margin(10) */
                    config_h_total = compute_config_total_height(es);
                } else {
                    config_h_total = section_hdr;
                }

                /*
                 * Cap the config panel so it never consumes more than half
                 * the right column, leaving room for the palette.  When
                 * content exceeds the cap, level_config_render clips and
                 * scrolls it.
                 */
                int config_h_max = (right_bottom - right_top) / 2;
                int config_h = config_h_total < config_h_max
                             ? config_h_total : config_h_max;

                /* Section 3 height (computed early so palette knows remaining space) */
                int props_h = 0;
                if (es->selection.index >= 0) {
                    props_h = es->panel_open ? 200 : section_hdr;
                }

                /* Section 2: Palette */
                int palette_y = config_y + config_h;
                int palette_h;
                if (es->palette_open) {
                    /* Palette gets remaining space minus properties */
                    palette_h = right_bottom - palette_y - props_h;
                    if (palette_h < section_hdr + 50) palette_h = section_hdr + 50;
                } else {
                    palette_h = section_hdr;  /* just the header */
                }

                /* Section 3: Properties (positioned after palette) */
                int props_y = palette_y + palette_h;

                /* Render in top-to-bottom order */
                level_config_render(es, config_y, config_h, config_h_total);
                palette_render(es, palette_y, palette_h);
                if (es->selection.index >= 0) {
                    properties_render(es, props_y, props_h);
                }
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
                (void)save_current_level(es);
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
                    open_level_file(es);
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
                (void)export_current_level(es);
                break;

            case SDLK_r:
                if (file_exists(es->autosave_path)) {
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
        if (mx >= CANVAS_W && my > TOOLBAR_H && my < EDITOR_H - STATUS_H) {
            int sc_section_hdr = 28;
            int sc_cfg_total   = sc_section_hdr;
            if (es->config_open) {
                sc_cfg_total = compute_config_total_height(es);
            }
            int sc_cfg_max = (EDITOR_H - STATUS_H - TOOLBAR_H) / 2;
            int sc_cfg_h   = sc_cfg_total < sc_cfg_max ? sc_cfg_total : sc_cfg_max;
            int sc_cfg_bot = TOOLBAR_H + sc_cfg_h;

            if (my < sc_cfg_bot)
                cfg_scroll(-event->wheel.y * 20);
            else
                palette_scroll(-event->wheel.y * 20);
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

/* ------------------------------------------------------------------ */
/* render_toolbar — static helper                                      */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* open_level_file / load_level_from_path — file import helpers        */
/* ------------------------------------------------------------------ */

/*
 * load_level_from_path — Load a TOML level file into the editor.
 *
 * Replaces the current level data, clears undo history, resets selection,
 * updates the file path and window title.  Called by both open_level_file
 * (from the dialog) and editor_main.c (from argv[1]).
 */
int editor_load_level(EditorState *es, const char *path) {
    LevelDef new_level;
    memset(&new_level, 0, sizeof(new_level));

    if (level_load_toml(path, &new_level) != 0) {
        fprintf(stderr, "Error: failed to load %s\n", path);
        editor_set_status(es, "Load failed: %s", path);
        return -1;
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
    add_recent_file(es, path);

    /*
     * Reload theme-dependent textures so the editor preview matches the
     * level's visual identity (floor tileset, foreground strip).
     */
    if (es->level.background_layer_count > 0) {
        const char *sky_path = es->level.background_layers[0].path;
        if (sky_path[0] != '\0') {
            SDL_Texture *new_sky = IMG_LoadTexture(es->renderer, sky_path);
            if (new_sky) {
                if (es->textures.sky)
                    SDL_DestroyTexture(es->textures.sky);
                es->textures.sky = new_sky;
            }
        }
    }
    if (es->level.floor_tile_path[0] != '\0') {
        SDL_Texture *new_floor = IMG_LoadTexture(es->renderer,
                                                  es->level.floor_tile_path);
        if (new_floor) {
            if (es->textures.floor_tile)
                SDL_DestroyTexture(es->textures.floor_tile);
            es->textures.floor_tile = new_floor;
        }
    }
    if (es->level.foreground_layer_count > 0) {
        const char *strip = es->level.foreground_layers[
            es->level.foreground_layer_count - 1].path;
        if (strip[0] != '\0') {
            SDL_Texture *new_water = IMG_LoadTexture(es->renderer, strip);
            if (new_water) {
                if (es->textures.water)
                    SDL_DestroyTexture(es->textures.water);
                es->textures.water = new_water;
            }
        }
    }

    /* Update the title bar to show the loaded file. */
    editor_update_window_title(es);

    fprintf(stderr, "Loaded %s (%d entities)\n", path,
            es->level.coin_count + es->level.spider_count +
            es->level.platform_count + es->level.rail_count +
            es->level.bird_count + es->level.fish_count);
    editor_set_status(es, "Loaded %s", path);
    return 0;
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
        (void)editor_load_level(es, path);
    }
    /* User cancelled — do nothing */
}

static int save_current_level(EditorState *es)
{
    if (!editor_can_persist(es, "Save")) return -1;

    if (es->file_path[0] == '\0') {
        strncpy(es->file_path, "levels/untitled.toml",
                sizeof(es->file_path) - 1);
        es->file_path[sizeof(es->file_path) - 1] = '\0';
    }
    if (level_save_toml(&es->level, es->file_path) == 0) {
        es->modified = 0;
        add_recent_file(es, es->file_path);
        editor_update_window_title(es);
        editor_set_status(es, "Saved %s", es->file_path);
        return 0;
    }

    fprintf(stderr, "Error: failed to save %s\n", es->file_path);
    editor_set_status(es, "Save failed: %s", es->file_path);
    return -1;
}

static int export_current_level(EditorState *es)
{
    const char *var_name = "untitled";
    char name_buf[128] = {0};

    if (!editor_can_persist(es, "Export")) return -1;

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
        editor_set_status(es, "Exported %s", var_name);
        return 0;
    }

    fprintf(stderr, "Error: export failed for '%s'\n", var_name);
    editor_set_status(es, "Export failed: %s", var_name);
    return -1;
}

static int file_exists(const char *path)
{
    FILE *fp;

    if (!path || path[0] == '\0') return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static void ensure_out_dirs(void)
{
#ifdef _WIN32
    _mkdir("out");
    _mkdir("out\\autosave");
#else
    mkdir("out", 0755);
    mkdir("out/autosave", 0755);
#endif
}

static void maybe_autosave(EditorState *es)
{
    Uint32 now = SDL_GetTicks();

    if (!es->modified) return;
    if (now - es->last_autosave_ms < EDITOR_AUTOSAVE_MS) return;

    es->last_autosave_ms = now;
    editor_validate_level(&es->level, &es->validation_report);
    if (es->validation_report.error_count > 0) return;

    ensure_out_dirs();
    if (level_save_toml(&es->level, es->autosave_path) == 0) {
        editor_set_status(es, "Autosaved %s", es->autosave_path);
    }
}

static void load_recent_files(EditorState *es)
{
    FILE *fp = fopen(EDITOR_RECENT_PATH, "r");
    char line[256];

    es->recent_file_count = 0;
    if (!fp) return;

    while (es->recent_file_count < EDITOR_RECENT_MAX &&
           fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (line[0] == '\0') continue;
        strncpy(es->recent_files[es->recent_file_count], line,
                sizeof(es->recent_files[0]) - 1);
        es->recent_files[es->recent_file_count][sizeof(es->recent_files[0]) - 1] = '\0';
        es->recent_file_count++;
    }
    fclose(fp);
}

static void save_recent_files(const EditorState *es)
{
    FILE *fp;

    ensure_out_dirs();
    fp = fopen(EDITOR_RECENT_PATH, "w");
    if (!fp) return;

    for (int i = 0; i < es->recent_file_count; i++) {
        fprintf(fp, "%s\n", es->recent_files[i]);
    }
    fclose(fp);
}

static void add_recent_file(EditorState *es, const char *path)
{
    int existing = -1;

    if (!path || path[0] == '\0') return;
    for (int i = 0; i < es->recent_file_count; i++) {
        if (strcmp(es->recent_files[i], path) == 0) {
            existing = i;
            break;
        }
    }

    if (existing > 0) {
        char tmp[256];
        strncpy(tmp, es->recent_files[existing], sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        for (int i = existing; i > 0; i--) {
            strncpy(es->recent_files[i], es->recent_files[i - 1],
                    sizeof(es->recent_files[i]) - 1);
            es->recent_files[i][sizeof(es->recent_files[i]) - 1] = '\0';
        }
        strncpy(es->recent_files[0], tmp, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
    } else if (existing < 0) {
        int limit = es->recent_file_count < EDITOR_RECENT_MAX
                  ? es->recent_file_count : EDITOR_RECENT_MAX - 1;
        for (int i = limit; i > 0; i--) {
            strncpy(es->recent_files[i], es->recent_files[i - 1],
                    sizeof(es->recent_files[i]) - 1);
            es->recent_files[i][sizeof(es->recent_files[i]) - 1] = '\0';
        }
        strncpy(es->recent_files[0], path, sizeof(es->recent_files[0]) - 1);
        es->recent_files[0][sizeof(es->recent_files[0]) - 1] = '\0';
        if (es->recent_file_count < EDITOR_RECENT_MAX) es->recent_file_count++;
    }

    save_recent_files(es);
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
    case ENT_FLOOR_GAP:
        es->clipboard_data.floor_gap = es->level.floor_gaps[i];
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
    case ENT_STAR_YELLOW:
        es->clipboard_data.star_yellow = es->level.star_yellows[i];
        break;
    case ENT_STAR_GREEN:
        es->clipboard_data.star_green = es->level.star_greens[i];
        break;
    case ENT_STAR_RED:
        es->clipboard_data.star_red = es->level.star_reds[i];
        break;
    case ENT_LAST_STAR:
        es->clipboard_data.last_star = es->level.last_star;
        break;
    case ENT_PLAYER_SPAWN: {
        /*
         * Player spawn is a single position (like last_star).
         * Reuse the last_star union member since both are {float x, float y}.
         */
        LastStarPlacement p = { es->level.player_start_x,
                                es->level.player_start_y };
        es->clipboard_data.last_star = p;
        break;
    }
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
    case ENT_BLUE_FLAME:
        es->clipboard_data.blue_flame = es->level.blue_flames[i];
        break;
    case ENT_FIRE_FLAME:
        es->clipboard_data.fire_flame = es->level.fire_flames[i];
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
    case ENT_STAR_YELLOW:
        d.star_yellow.x += PASTE_OFFSET;
        d.star_yellow.y += PASTE_OFFSET;
        PASTE_INTO(star_yellows, star_yellow_count, MAX_STAR_YELLOWS, star_yellow);
        break;
    case ENT_STAR_GREEN:
        d.star_green.x += PASTE_OFFSET;
        d.star_green.y += PASTE_OFFSET;
        PASTE_INTO(star_greens, star_green_count, MAX_STAR_YELLOWS, star_green);
        break;
    case ENT_STAR_RED:
        d.star_red.x += PASTE_OFFSET;
        d.star_red.y += PASTE_OFFSET;
        PASTE_INTO(star_reds, star_red_count, MAX_STAR_YELLOWS, star_red);
        break;
    case ENT_LAST_STAR:
        d.last_star.x += PASTE_OFFSET;
        d.last_star.y += PASTE_OFFSET;
        es->level.last_star = d.last_star;
        es->modified = 1;
        break;
    case ENT_PLAYER_SPAWN:
        /*
         * Player spawn is a single position — paste overwrites the
         * existing position, offset from the copied location.
         */
        es->level.player_start_x = d.last_star.x + PASTE_OFFSET;
        es->level.player_start_y = d.last_star.y + PASTE_OFFSET;
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
    case ENT_BLUE_FLAME:
        d.blue_flame.x += PASTE_OFFSET;
        PASTE_INTO(blue_flames, blue_flame_count, MAX_BLUE_FLAMES, blue_flame);
        break;
    case ENT_FIRE_FLAME:
        d.fire_flame.x += PASTE_OFFSET;
        PASTE_INTO(fire_flames, fire_flame_count, MAX_BLUE_FLAMES, fire_flame);
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
    case ENT_FLOOR_GAP:
        d.floor_gap += (int)PASTE_OFFSET;
        if (es->level.floor_gap_count < MAX_FLOOR_GAPS) {
            int idx = es->level.floor_gap_count;
            es->level.floor_gaps[idx] = d.floor_gap;
            es->level.floor_gap_count++;
            es->selection.type = t;
            es->selection.index = idx;
            es->modified = 1;
            Command cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.type = CMD_PLACE;
            cmd.entity_type = (int)t;
            cmd.entity_index = idx;
            cmd.after.floor_gap = d.floor_gap;
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
 *   1. Export the current level as src/levels/exported/<name>.c/.h.
 *   2. Auto-save TOML if a path is set.
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
    if (!editor_can_persist(es, "Playtest")) return;

    /*
     * Step 1 — Save the level as TOML.
     *
     * The game accepts --level <path> to load any TOML file.
     * If the editor has a file path, save and play from there.
     * Otherwise save to a temporary file so the game has something to load.
     */
    const char *save_path = es->file_path[0] != '\0'
                          ? es->file_path
                          : "levels/_playtest.toml";

    if (level_save_toml(&es->level, save_path) != 0) {
        fprintf(stderr, "Play: failed to save %s\n", save_path);
        editor_set_status(es, "Play failed: save %s", save_path);
        return;
    }
    es->modified = 0;
    editor_set_status(es, "Play saved %s", save_path);

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
        /* Child process — become the game, passing the level file */
        if (es->debug_play)
            execl("./out/super-mango", "super-mango",
                  "--level", save_path, "--debug", (char *)NULL);
        else
            execl("./out/super-mango", "super-mango",
                  "--level", save_path, (char *)NULL);
        /* execl only returns on error */
        _exit(1);
    } else if (pid > 0) {
        /* Parent process — record the child and enter play mode */
        es->play_pid = (int)pid;
        es->playing = 1;
        editor_set_status(es, "Play launched %s", save_path);
        SDL_SetWindowTitle(es->window, "Super Mango Editor - Playing...");
    } else {
        fprintf(stderr, "Play: fork() failed\n");
        editor_set_status(es, "Play failed: fork");
    }
#else
    /* Windows: use system() with start to launch non-blocking */
    {
        char cmd[512];
        int rc;
        snprintf(cmd, sizeof(cmd),
                 "start /B .\\out\\super-mango.exe --level \"%s\"%s",
                 save_path, es->debug_play ? " --debug" : "");
        rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "Play: launch failed for %s (rc=%d)\n", save_path, rc);
            editor_set_status(es, "Play failed: launch %s", save_path);
            return;
        }
    }
    es->playing = 1;
    editor_set_status(es, "Play launched %s", save_path);
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
    editor_set_status(es, "Play stopped");

    /* Restore the editor title bar. */
    editor_update_window_title(es);
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
        if (result > 0) {
            /* Child exited (either normally or via signal) */
            es->play_pid = 0;
            es->playing = 0;
            if (WIFEXITED(status)) {
                editor_set_status(es, "Play exited: code %d", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                editor_set_status(es, "Play exited: signal %d", WTERMSIG(status));
            } else {
                editor_set_status(es, "Play ended");
            }

            /* Restore the editor title. */
            editor_update_window_title(es);
        } else if (result < 0) {
            if (errno == ECHILD) {
                es->play_pid = 0;
                es->playing = 0;
                editor_set_status(es, "Play ended: process already reaped");

                /* Restore the editor title. */
                editor_update_window_title(es);
            } else {
                editor_set_status(es, "Play status check failed");
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

    /* ---- Grid toggle button ----------------------------------------- */
    bx += bw + 20;
    const char *grid_label = es->show_grid ? "[Grid]" : " Grid ";
    if (ui_button(&es->ui, bx, by, 56, bh, grid_label)) {
        es->show_grid ^= 1;
    }

    /* ---- Debug toggle button ---------------------------------------- */
    bx += 60;
    {
        const char *dbg_label = es->debug_play ? "[Debug]" : " Debug ";
        if (ui_button(&es->ui, bx, by, 56, bh, dbg_label)) {
            es->debug_play ^= 1;
        }
    }

    /* ---- Zoom dropdown ---------------------------------------------- */
    bx += 60;
    {
        static const char *zoom_opts[] = { "Zoom: 1x", "Zoom: 2x", "Zoom: 3x", "Zoom: 5x" };
        static const float zoom_vals[] = { 1.0f, 2.0f, 3.0f, 5.0f };
        static const int zoom_count = 4;

        int sel = 1;
        for (int zi = 0; zi < zoom_count; zi++) {
            if (es->camera.zoom == zoom_vals[zi]) { sel = zi; break; }
        }
        if (ui_dropdown(&es->ui, 8888, bx, by + 2, 80,
                         zoom_opts, zoom_count, &sel)) {
            es->camera.zoom = zoom_vals[sel];
        }
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
        (void)export_current_level(es);
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Save")) {
        (void)save_current_level(es);
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "Open")) {
        /*
         * Open button — show the native file dialog and load the selected
         * TOML level file.  Same behaviour as Ctrl+O.
         */
        if (editor_confirm_discard_changes(es, "open another level")) {
            open_level_file(es);
        }
    }

    rx -= 64 + 4;
    if (ui_button(&es->ui, rx, by, 64, bh, "New")) {
        if (editor_confirm_discard_changes(es, "create a new level")) {
            editor_reset_new_level(es);
        }
    }
}

/* ------------------------------------------------------------------ */
/* render_status_bar — static helper                                   */
/* ------------------------------------------------------------------ */

/*
 * render_status_bar — Draw the bottom status bar (32 px tall, full width).
 *
 * Layout from left to right:
 *   Mouse: (worldX, worldY)  |  Tool: Select  |  Entities: 42  |  path.toml *
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
    ui_label(&es->ui, 210, bar_y + 8, tool_text);

    ui_label_color(&es->ui, 330, bar_y + 8,
                   editor_validation_summary(&es->validation_report),
                   es->validation_report.error_count > 0 ?
                   (SDL_Color){0xFF,0x70,0x70,0xFF} : UI_TEXT_DIM);

    /* ---- Right: entity count and file info --------------------------- */
    /*
     * Sum up all entity counts in the LevelDef to show the total number
     * of placed entities.  This gives a quick sense of level complexity.
     */
    int total = es->level.floor_gap_count
              + es->level.rail_count
              + es->level.platform_count
              + es->level.coin_count
              + es->level.star_yellow_count
              + es->level.star_green_count
              + es->level.star_red_count
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

    char info_text[512];
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
    ui_label(&es->ui, 600, bar_y + 8, info_text);
    if (es->status_message[0] != '\0') {
        ui_label_color(&es->ui, 920, bar_y + 8, es->status_message, UI_TEXT_DIM);
    }
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

    case ENT_STAR_YELLOW:
        APPLY_ARRAY(es->level.star_yellows, es->level.star_yellow_count,
                     star_yellow, MAX_STAR_YELLOWS);
        break;

    case ENT_STAR_GREEN:
        APPLY_ARRAY(es->level.star_greens, es->level.star_green_count,
                     star_green, MAX_STAR_YELLOWS);
        break;

    case ENT_STAR_RED:
        APPLY_ARRAY(es->level.star_reds, es->level.star_red_count,
                     star_red, MAX_STAR_YELLOWS);
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

    case ENT_PLAYER_SPAWN:
        /*
         * Player spawn is a single position (like last_star).  Undo/redo
         * overwrites the position directly from the before/after snapshot.
         * We reuse the last_star union member since both are {float x, y}.
         */
        if (reverse) {
            es->level.player_start_x = cmd->before.last_star.x;
            es->level.player_start_y = cmd->before.last_star.y;
        } else {
            es->level.player_start_x = cmd->after.last_star.x;
            es->level.player_start_y = cmd->after.last_star.y;
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

    case ENT_BLUE_FLAME:
        APPLY_ARRAY(es->level.blue_flames, es->level.blue_flame_count,
                     blue_flame, MAX_BLUE_FLAMES);
        break;

    case ENT_FIRE_FLAME:
        APPLY_ARRAY(es->level.fire_flames, es->level.fire_flame_count,
                     fire_flame, MAX_BLUE_FLAMES);
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

    case ENT_FLOOR_GAP:
        APPLY_ARRAY(es->level.floor_gaps, es->level.floor_gap_count,
                     floor_gap, MAX_FLOOR_GAPS);
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
    /* Destroy all renderer-owned preview textures before the renderer. */
    editor_textures_cleanup(es);

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
