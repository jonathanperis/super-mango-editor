/*
 * editor.c — Core implementation of the Super Mango level editor.
 *
 * Implements the three lifecycle functions declared in editor.h:
 *   editor_init    — create window, renderer, font, textures, undo stack.
 *   editor_loop    — main event/render loop at 60 FPS.
 *   editor_cleanup — free every resource in reverse init order.
 *
 * Per-frame update/render work lives in editor_frame.c so this file stays
 * focused on startup, the loop boundary, and teardown.
 *
 * The editor follows the same single-struct-by-pointer pattern as the game:
 * EditorState holds every resource, and is passed to every function.
 */

#include <SDL.h>          /* SDL_Window, SDL_Renderer, SDL_Event, etc.     */
#include <SDL_ttf.h>      /* TTF_OpenFont, TTF_CloseFont — text rendering  */
#include <stdio.h>        /* fprintf, stderr, snprintf — error and status  */
#include <string.h>       /* strncpy — string operations                    */

#include "editor.h"       /* EditorState, constants, EntityType, EditorTool */
#include "editor_frame.h" /* editor_run_frame — one frame of editor work    */
#include "undo.h"         /* UndoStack, undo_create/destroy/push/pop/clear  */
#include "ui.h"           /* UIState, ui_init, ui_begin_frame, ui_button    */
#include "editor_files.h" /* editor file/save/export/autosave helpers       */
#include "editor_session.h" /* editor status/title/session helpers           */
#include "editor_textures.h" /* editor_textures_load/cleanup                 */

#define EDITOR_AUTOSAVE_PATH "out/autosave/editor_autosave.toml"

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
     * "assets/fonts/round9x13.ttf" is the same monospaced pixel font the game's
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
    editor_load_recent_files(es);

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
    if (editor_file_exists(es->autosave_path)) {
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
    while (es->running) {
        editor_run_frame(es);
    }
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
