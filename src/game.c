/*
 * game.c — Window, renderer, background, and main game loop.
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h> /* strncpy, memset */

#include "game.h"
#include "player/player.h"
#include "effects/fog.h"
#include "screens/hud.h"
#include "levels/level.h"         /* LevelDef struct                                    */
#include "levels/level_loader.h"  /* level_load, level_reset                            */
#include "levels/level_path.h"    /* level_resolve_path                                */
#include "levels/level_resources.h" /* level-specific resource reloads                  */
#include "levels/phase_transition.h" /* phase next path/progress helpers                 */
#include "editor/serializer.h"    /* level_load_toml                                    */
#include "render/game_render.h"        /* game_render_frame, render overlays         */
#include "input/game_input.h"          /* gamepad deferred init/cleanup              */
#include "input/game_events.h"         /* game_handle_events                         */
#include "core/game_resources.h"       /* game_resources_load/cleanup                */
#include "core/game_update.h"          /* game_update_active                         */
#include "core/game_completion.h"      /* game_completion_reset_summary              */
#include "core/game_timing.h"          /* frame timing and smoke-test helpers         */

/* ------------------------------------------------------------------ */
/* Level data — loaded once from TOML, reused on player death resets    */
/* ------------------------------------------------------------------ */

/*
 * File-scoped level definition.  Populated once from TOML in game_init,
 * then referenced by reset_current_level on player death.
 */
LevelDef s_level;

/* ------------------------------------------------------------------ */

static void game_init_fail(GameState *gs, const char *label, const char *detail)
{
    fprintf(stderr, "%s: %s\n", label, detail);
    game_cleanup(gs);
    exit(EXIT_FAILURE);
}

/*
 * game_init — Set up everything the game needs before the loop starts.
 *
 * Called once at startup. Creates the OS window, the GPU renderer,
 * loads the background image, and initialises the player.
 */
void game_init(GameState *gs) {
    /*
     * SDL_CreateWindow — ask the OS to create a window.
     *   WINDOW_TITLE            → text shown in the title bar
     *   SDL_WINDOWPOS_CENTERED  → center the window on the monitor (x and y)
     *   WINDOW_W / WINDOW_H     → 800×600 pixels
     *   SDL_WINDOW_SHOWN        → make it visible immediately
     */
    gs->window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );
    if (!gs->window) {
        game_init_fail(gs, "SDL_CreateWindow error", SDL_GetError());
    }

    /*
     * Nearest-neighbour pixel scaling — must be set before SDL_CreateRenderer.
     * Value "0" = SDL_ScaleModeNearest: each source pixel maps to an exact
     * NxN block of destination pixels, preserving the crisp look of pixel art.
     * The default ("linear") blurs sprites when they are scaled, which makes
     * small pixel-art sprites (e.g. 16×16 fish → 48×48 on screen) look wrong.
     */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    /*
     * SDL_CreateRenderer — create a 2D drawing context tied to the window.
     *   -1 → let SDL pick the best available GPU driver automatically
     *   SDL_RENDERER_ACCELERATED  → use the GPU (not software fallback)
     *   SDL_RENDERER_PRESENTVSYNC → lock present() to the monitor refresh rate
     *                               (prevents screen tearing and runaway CPU)
     */
    gs->renderer = SDL_CreateRenderer(
        gs->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!gs->renderer) {
        /* Dummy/headless drivers may not expose accelerated renderers. */
        gs->renderer = SDL_CreateRenderer(gs->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!gs->renderer) {
        game_init_fail(gs, "SDL_CreateRenderer error", SDL_GetError());
    }

    /*
     * SDL_RenderSetLogicalSize — define the internal canvas resolution.
     * All SDL render calls now use GAME_W × GAME_H coordinates (400×300).
     * SDL scales this canvas up to the OS window size (800×600) automatically,
     * giving every pixel a 2×2 block on screen — the chunky pixel-art look.
     */
    SDL_RenderSetLogicalSize(gs->renderer, GAME_W, GAME_H);

    game_resources_load(gs);

    /* Set up the player (loads texture, sets initial position on the floor) */
    player_init(&gs->player, gs->renderer);

    /* Camera starts at the far-left edge of the world. */
    gs->camera.x = 0.0f;

    /*
     * Fog textures are loaded later, after level_load, because the fog
     * texture paths come from the level definition (fog_layers in LevelDef).
     */

    /* Load the HUD font and heart icon texture */
    hud_init(&gs->hud, gs->renderer, gs->textures.star_yellow, gs->player.texture);

    /* Initialise the debug overlay if --debug was passed on the CLI */
    if (gs->debug_mode) debug_init(&gs->debug);

    /* Load level from TOML if a path was provided via --level */
    memset(&s_level, 0, sizeof(s_level));

    char safe_path[512] = {0};
    int path_valid = (level_resolve_path(gs->level_path, safe_path, sizeof(safe_path)) == 0);

    level_def_init_defaults(&s_level);

    if (path_valid &&
        level_load_toml(safe_path, &s_level) == 0) {
        /* Successfully loaded from the resolved path */
    } else {
        if (gs->level_path[0] != '\0')
            fprintf(stderr, "Warning: could not load %s — starting empty level\n",
                    gs->level_path);
        strncpy(s_level.name, "Untitled", sizeof(s_level.name) - 1);
    }
    level_load(gs, &s_level);
    game_completion_reset_summary(gs);

    level_resources_apply(gs, (const LevelDef *)gs->runtime.current_level);

    /*
     * Health/lives/score are now set by level_load() from LevelDef fields
     * (initial_hearts, initial_lives, score_per_life).  No hardcoded init
     * needed here — level_load handles it.
     */

    gamepad_schedule_deferred_init(gs);

    /* Signal the loop to start running; game starts in the foreground */
    gs->running = 1;
    gs->paused  = 0;
    gs->level_complete = 0;
    gs->checkpoint_x = 0.0f;
}

/* ------------------------------------------------------------------ */

/*
 * game_loop — The heart of the game. Runs until gs->running == 0.
 *
 * Every iteration of the while loop is one "frame". Each frame:
 *   1. Calculate how much time has passed (delta time).
 *   2. Process all pending OS/input events.
 *   3. Update game state (player position, etc.).
 *   4. Draw everything to the screen.
 */

/* ------------------------------------------------------------------ */

/*
 * game_load_next_phase — Load the next level when last_star is collected.
 *
 * Called from game_collide when the player touches the last_star and
 * next_phase is set. Reloads the level while preserving score, lives, and
 * hearts. Resets checkpoint and player position to the new level start.
 *
 * Returns 0 on success, -1 on failure (level not found or load error).
 */
int game_load_next_phase(GameState *gs)
{
    const LevelDef *current = (const LevelDef *)gs->runtime.current_level;
    char next_path[256] = {0};
    if (phase_next_path(current, next_path, sizeof(next_path)) != 0) return -1;

    /* Save player progress */
    PhaseProgress saved_progress;
    phase_progress_save(gs, &saved_progress);

    char safe_path[512] = {0};
    if (level_resolve_path(next_path, safe_path, sizeof(safe_path)) != 0) {
        fprintf(stderr, "Error: Failed to resolve next phase path: %s\n", next_path);
        return -1;
    }

    /* Load the next level */
    level_def_init_defaults(&s_level);

    if (level_load_toml(safe_path, &s_level) != 0) {
        fprintf(stderr, "Error: Failed to load next phase: %s\n", safe_path);
        return -1;
    }

    /* Update the level path for the game state */
    strncpy(gs->level_path, next_path, sizeof(gs->level_path) - 1);
    gs->level_path[sizeof(gs->level_path) - 1] = '\0';

    /* Load the new level */
    level_load(gs, &s_level);
    game_completion_reset_summary(gs);
    level_resources_apply(gs, &s_level);

    /* Restore player progress */
    phase_progress_restore(gs, &saved_progress);

    /* Reset checkpoint and completion flag */
    gs->checkpoint_x = 0.0f;
    gs->level_complete = 0;

    if (gs->debug_mode) {
        debug_log(&gs->debug, "PHASE TRANSITION to: %s", safe_path);
    }

    return 0;
}

/* ------------------------------------------------------------------ */

/*
 * game_loop_frame — Execute one frame of the game loop.
 *
 * Extracted from game_loop so it can be called either in a blocking
 * while loop (native) or as an emscripten_set_main_loop_arg callback
 * (WebAssembly).  The void* parameter is cast back to GameState*.
 */
static void game_loop_frame(void *arg) {
    GameState *gs = (GameState *)arg;

    const Uint32 frame_ms = 1000 / TARGET_FPS;

    {
        Uint64 frame_start_ticks = 0;
        float dt = game_timing_step(gs, &frame_start_ticks);

        /* ---- 1. Events ------------------------------------------- */
        game_handle_events(gs);

        /* ---- 2. Update ------------------------------------------- */
        /*
         * cam_x is declared here so it is in scope for both the paused and
         * the active paths.  When paused we jump straight to render with the
         * camera frozen at its last position; when active the camera-update
         * block below overwrites it with the new smoothed value.
         */
        int cam_x = (int)gs->camera.x;

        /*
         * Skip all physics and game-logic updates while the window is in the
         * background.  The render step still runs so the last frame stays
         * visible in the taskbar thumbnail and on the screen.
         * Also skip updates when level is complete (overlay showing).
         */
        if (gs->paused || gs->level_complete) goto render;

        cam_x = game_update_active(gs, dt, cam_x);

        /* ---- 3. Render ------------------------------------------- */
        render:
        game_render_frame(gs, cam_x, dt);

        /*
         * Gamepad init state machine — runs non-blocking across multiple frames.
         *
         * State 1: first frame just presented — spawn the background thread.
         * State 2: thread running — check each frame if it has finished.
         *          When done, open the controller on the main thread (thread-safe)
         *          and clear ctrl_pending_init so the HUD message disappears.
         *
         * WebAssembly is handled inside gamepad_update_deferred_init(); native
         * builds run the deferred SDL_INIT_GAMECONTROLLER path there.
         */
        gamepad_update_deferred_init(gs);

        game_timing_cap_frame(frame_start_ticks, frame_ms);
        game_timing_tick_smoke(gs);
    }
}

/* ------------------------------------------------------------------ */

/*
 * game_loop — Run the game until gs->running becomes 0.
 *
 * On native platforms this is a blocking while loop.
 * On Emscripten (WebAssembly) we hand control to the browser's
 * requestAnimationFrame via emscripten_set_main_loop_arg, which
 * calls game_loop_frame once per vsync.
 */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void game_loop(GameState *gs) {
    gs->loop.prev_ticks = SDL_GetTicks64();
    gs->loop.fp_prev_riding  = -1;

#ifdef __EMSCRIPTEN__
    /*
     * Flush stale keyboard state before the first game frame.
     *
     * SDL2 registers JavaScript keydown/keyup listeners on #canvas during
     * SDL_Init and maintains an internal keystate[] array directly (not just
     * the SDL event queue).  If any spurious keydown fired during Emscripten
     * module initialisation — e.g. from a browser navigation/focus event when
     * INVOKE_RUN=1 calls main() synchronously — the matching keyup is never
     * delivered and that scancode stays SDL_PRESSED for the entire session.
     * SDL_PollEvent drains the event queue but does NOT reset keystate[], so
     * the stuck key persists across all frames, causing the player to walk
     * indefinitely without any physical input.
     *
     * Fix: dispatch synthetic keyup events for every game input key on the
     * canvas immediately before the Emscripten main loop starts.
     * dispatchEvent() is synchronous — SDL's listener runs immediately and
     * sets those scancodes to SDL_RELEASED before the first frame fires.
     * SDL_FlushEvents then discards the resulting SDL_KEYUP entries queued by
     * those listeners so they do not appear as noise on frame 1.
     */
    /*
     * emscripten_run_script — plain C function (no GNU extension required),
     * compatible with -std=c11 unlike EM_ASM which needs -std=gnu*.
     * Evaluates the JavaScript string synchronously in the browser context.
     */
    emscripten_run_script(
        "(function(){"
        "  var K=['ArrowLeft','ArrowRight','ArrowUp','ArrowDown',"
        "    'Space','KeyA','KeyD','KeyW','KeyS','ShiftLeft','ShiftRight'];"
        "  var c=document.getElementById('canvas');"
        "  if(!c)return;"
        "  K.forEach(function(code){"
        "    c.dispatchEvent(new KeyboardEvent('keyup',{"
        "      code:code,bubbles:true,cancelable:true"
        "    }));"
        "  });"
        "})();"
    );
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    /*
     * emscripten_set_main_loop_arg — register a per-frame callback.
     *   arg 1: callback function (receives void* user data)
     *   arg 2: user data pointer (our GameState)
     *   arg 3: target FPS (0 = use requestAnimationFrame, recommended)
     *   arg 4: simulate_infinite_loop (1 = never return from this call)
     */
    emscripten_set_main_loop_arg(game_loop_frame, gs, 0, 1);
#else
    while (gs->running) {
        game_loop_frame(gs);
    }
#endif
}

/* ------------------------------------------------------------------ */

/*
 * game_cleanup — Free every resource owned by the game.
 *
 * Always destroy in reverse init order, because later objects may
 * depend on earlier ones (e.g. a texture requires the renderer to exist).
 * After destroying, we set pointers to NULL so accidental double-frees
 * are safe (SDL_Destroy* and free() on NULL are no-ops).
 */
void game_cleanup(GameState *gs) {
    /*
     * Close the gamepad and shut down the controller subsystem.
     *
     * SDL_GameControllerClose releases the handle for this specific device.
     * SDL_QuitSubSystem mirrors the SDL_InitSubSystem call in game_init —
     * it decrements the internal reference count for SDL_INIT_GAMECONTROLLER
     * and shuts the subsystem down when the count reaches zero.
     * Both calls are safe when their argument is NULL / the subsystem is
     * not active, so no extra guard is needed beyond the pointer check.
     */
    gamepad_cleanup(gs);

    /* Free HUD resources (font + star texture, renderer-dependent) */
    hud_cleanup(&gs->hud);

    /* Free fog textures (renderer-dependent, free before renderer) */
    fog_cleanup(&gs->fog);

    /* Free the player's texture first (also renderer-dependent) */
    player_cleanup(&gs->player);

    game_resources_cleanup(gs);

    if (gs->renderer) {
        SDL_DestroyRenderer(gs->renderer);
        gs->renderer = NULL;
    }

    if (gs->window) {
        SDL_DestroyWindow(gs->window);
        gs->window = NULL;
    }
}
