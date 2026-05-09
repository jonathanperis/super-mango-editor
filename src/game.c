/*
 * game.c — Window, renderer, background, and main game loop.
 */

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h> /* strncpy, memset */

#if defined(_WIN32)
#include <windows.h> /* GetFullPathNameA */
#elif !defined(__EMSCRIPTEN__)
#include <limits.h>  /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

#include "game.h"
#include "player/player.h"
#include "surfaces/platform.h"
#include "effects/water.h"
#include "effects/fog.h"
#include "entities/spider.h"
#include "entities/fish.h"
#include "collectibles/coin.h"
#include "surfaces/vine.h"
#include "surfaces/bouncepad.h"
#include "surfaces/bouncepad_small.h"
#include "surfaces/bouncepad_medium.h"
#include "surfaces/bouncepad_high.h"
#include "screens/hud.h"
#include "effects/parallax.h"
#include "surfaces/rail.h"
#include "hazards/spike_block.h"
#include "surfaces/float_platform.h"
#include "collectibles/star_yellow.h"
#include "hazards/axe_trap.h"
#include "hazards/circular_saw.h"
#include "hazards/blue_flame.h"
#include "surfaces/ladder.h"
#include "surfaces/rope.h"
#include "entities/faster_fish.h"
#include "collectibles/last_star.h"
#include "hazards/spike.h"
#include "hazards/spike_platform.h"
#include "levels/level.h"         /* LevelDef struct                                    */
#include "levels/level_loader.h"  /* level_load, level_reset                            */
#include "levels/level_resources.h" /* level-specific resource reloads                  */
#include "levels/phase_transition.h" /* phase next path/progress helpers                 */
#include "editor/serializer.h"    /* level_load_toml                                    */
#include "collision/game_collision.h"  /* game_collide, hitbox builders               */
#include "collision/floor_gap_collision.h" /* floor_gap_handle_collision              */
#include "render/game_render.h"        /* game_render_frame, render overlays         */
#include "core/game_state.h"           /* reset_current_level                        */
#include "input/game_input.h"          /* ctrl_init_worker                           */
#include "input/game_events.h"         /* game_handle_events                         */
#include "core/game_resources.h"       /* game_resources_load/cleanup                */
#include "core/game_camera.h"          /* game_camera_update                         */
#include "core/game_bouncepads.h"      /* combined bouncepad list/hit response       */
#include "core/game_checkpoint.h"      /* game_checkpoint_update                     */
#include "core/game_bridges.h"         /* game_bridges_update                        */
#include "core/game_float_platforms.h" /* game_float_platforms_update                */

/* ------------------------------------------------------------------ */
/* Level data — loaded once from TOML, reused on player death resets    */
/* ------------------------------------------------------------------ */

/*
 * File-scoped level definition.  Populated once from TOML in game_init,
 * then referenced by reset_current_level on player death.
 */
LevelDef s_level;

/*
 * File-scoped combined bouncepad array — avoids allocating 48 structs
 * on the stack every frame.  Populated once per frame in game_loop_frame.
 */
static Bouncepad s_all_pads[MAX_BOUNCEPADS_MEDIUM + MAX_BOUNCEPADS_SMALL + MAX_BOUNCEPADS_HIGH];

/* ------------------------------------------------------------------ */

static void game_init_fail(GameState *gs, const char *label, const char *detail)
{
    fprintf(stderr, "%s: %s\n", label, detail);
    game_cleanup(gs);
    exit(EXIT_FAILURE);
}

static int game_count_collected_coins(const GameState *gs)
{
    int collected = 0;

    for (int i = 0; i < gs->coin_count; i++) {
        if (!gs->coins[i].active) collected++;
    }

    return collected;
}

static void game_reset_completion_summary(GameState *gs)
{
    gs->level_elapsed = 0.0f;
    gs->level_coin_total = gs->coin_count;
    gs->completion_coins_collected = 0;
    gs->completion_coin_total = gs->coin_count;
    gs->completion_elapsed = 0.0f;
    gs->completion_pending_next_phase = 0;
    gs->completion_next_phase[0] = '\0';
}

void game_complete_level(GameState *gs)
{
    const LevelDef *def = (const LevelDef *)gs->runtime.current_level;

    gs->completion_coins_collected = game_count_collected_coins(gs);
    gs->completion_coin_total = gs->level_coin_total;
    gs->completion_elapsed = gs->level_elapsed;
    gs->completion_pending_next_phase = 0;
    gs->completion_next_phase[0] = '\0';

    if (phase_has_next(def) &&
        phase_next_path(def, gs->completion_next_phase,
                        sizeof(gs->completion_next_phase)) == 0) {
        gs->completion_pending_next_phase = 1;
    }

    gs->level_complete = 1;
}

/* ------------------------------------------------------------------ */

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

    /*
     * Validate the level path before opening.  Resolve the path to its
     * canonical form with realpath() so symbolic links and ".." are
     * eliminated, then pass only the resolved path to fopen.
     * This satisfies CodeQL's taint analysis for user-controlled paths.
     */
    char safe_path[512] = {0};
    int path_valid = 0;
    if (gs->level_path[0] != '\0') {
#if defined(__EMSCRIPTEN__)
        /* Emscripten has no realpath — use the path as-is */
        strncpy(safe_path, gs->level_path, sizeof(safe_path) - 1);
        path_valid = 1;
#elif defined(_WIN32)
        char resolved[260];  /* MAX_PATH */
        if (GetFullPathNameA(gs->level_path, 260, resolved, NULL)) {
            strncpy(safe_path, resolved, sizeof(safe_path) - 1);
            path_valid = 1;
        }
#else
        char resolved[PATH_MAX];
        if (realpath(gs->level_path, resolved) != NULL) {
            strncpy(safe_path, resolved, sizeof(safe_path) - 1);
            path_valid = 1;
        }
#endif
    }

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
    game_reset_completion_summary(gs);

    level_resources_apply(gs, (const LevelDef *)gs->runtime.current_level);

    /*
     * Health/lives/score are now set by level_load() from LevelDef fields
     * (initial_hearts, initial_lives, score_per_life).  No hardcoded init
     * needed here — level_load handles it.
     */

    /*
     * Lazy gamepad initialisation — deferred from SDL_Init on purpose.
     *
     * SDL_InitSubSystem activates one SDL subsystem after the main SDL_Init
     * call.  We do this here, after the window is already visible, instead of
     * at process startup.  Antivirus software applies stricter heuristics to
     * code that enumerates HID / XInput devices during the very first frames
     * of a new process; waiting until the game window exists sidesteps those
     * false-positive triggers.
     *
     * Non-fatal: if the subsystem fails to start (e.g. driver issue) the game
     * continues with keyboard-only input.
     */
    /*
     * Gamepad init is deferred to the first frame of the game loop.
     * SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) enumerates HID devices,
     * which can block for 20-30 seconds on Windows when antivirus software
     * is active. Deferring keeps game_init fast so the window appears immediately.
     *
     * Disabled on WebAssembly: Emscripten has no real pthreads support in this
     * build, SDL_CreateThread would run synchronously, and initialising the
     * gamepad subsystem via the browser Gamepad API can generate unexpected SDL
     * events that corrupt the keyboard state before the first frame. Controller
     * input is already disabled on WASM (see #ifndef __EMSCRIPTEN__ in
     * player_handle_input), so there is nothing to gain from running this here.
     */
#ifndef __EMSCRIPTEN__
    if (gs->smoke_test_frames == 0) {
        gs->ctrl_pending_init = 1;
    }
#endif

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

    /* Load the next level */
    level_def_init_defaults(&s_level);

    char safe_path[512] = {0};
    strncpy(safe_path, next_path, sizeof(safe_path) - 1);

    if (level_load_toml(safe_path, &s_level) != 0) {
        fprintf(stderr, "Error: Failed to load next phase: %s\n", safe_path);
        return -1;
    }

    /* Update the level path for the game state */
    strncpy(gs->level_path, next_path, sizeof(gs->level_path) - 1);
    gs->level_path[sizeof(gs->level_path) - 1] = '\0';

    /* Load the new level */
    level_load(gs, &s_level);
    game_reset_completion_summary(gs);
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
        /*
         * Delta time (dt) — seconds elapsed since the previous frame.
         * SDL_GetTicks64() returns milliseconds since SDL was initialised.
         * Dividing by 1000 converts to seconds (a float like 0.016).
         * We multiply velocities by dt so movement is frame-rate independent.
         */
        Uint64 now = SDL_GetTicks64();
        float  dt  = (float)(now - gs->loop.prev_ticks) / 1000.0f;
        gs->loop.prev_ticks = now;

        /*
         * Delta-time guard — cap to 100 ms (≈ 10 fps minimum).
         * When the OS moves the window to the background it can pause or
         * throttle the process for hundreds of milliseconds.  Without this
         * cap the first frame after a focus-loss produces a huge dt that
         * sends physics haywire (entities teleport, hurt_timer expires
         * instantly, collisions pile up) and resets the game state.
         * The same spike happens on Windows when the user drags the window,
         * which blocks the event loop for the duration of the drag.
         */
        if (dt > 0.1f) dt = 0.1f;

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

        gs->level_elapsed += dt;

        /* Read keyboard and gamepad; set the player's velocity for this frame */
        player_handle_input(&gs->player, gs->audio.jump, gs->controller,
                            gs->vines, gs->vine_count,
                            gs->ladders, gs->ladder_count,
                            gs->ropes, gs->rope_count);

        /*
         * Move the player, resolve floor + platform + float-platform +
         * bouncepad collisions.
         * bounce_idx is set to the bouncepad hit this frame, or -1.
         * fp_landed_idx is set to the float platform the player landed on,
         * or -1 if none.  Both must be initialised to -1 before the call.
         */
        int all_pad_count = game_bouncepads_collect(gs, s_all_pads);

        int bounce_idx    = -1;
        int fp_landed_idx = -1;
        player_update(&gs->player, dt, gs->audio.jump,
                      gs->platforms, gs->platform_count,
                      gs->float_platforms, gs->float_platform_count,
                      s_all_pads, all_pad_count,
                      gs->vines, gs->vine_count,
                      gs->ladders, gs->ladder_count,
                      gs->ropes, gs->rope_count,
                      gs->bridges, gs->bridge_count,
                      gs->spike_platforms, gs->spike_platform_count,
                      gs->floor_gaps, gs->floor_gap_count,
                      &bounce_idx, &fp_landed_idx,
                      gs->loop.fp_prev_riding,
                      gs->runtime.world_w);

        /*
         * Bouncepad landing response: start the release animation and play
         * the spring sound.  The launch impulse itself was already applied
         * inside player_update so the player is already moving upward.
         */
        game_bouncepads_handle_hit(gs, bounce_idx);
        floor_gap_handle_collision(gs);

        /* Move spiders along their patrol paths and advance their animation */
        spiders_update(gs->spiders, gs->spider_count, dt,
                       gs->floor_gaps, gs->floor_gap_count);
        /* Move jumping spiders: patrol + periodic jump arcs */
        jumping_spiders_update(gs->jumping_spiders, gs->jumping_spider_count, dt,
                               gs->floor_gaps, gs->floor_gap_count,
                               gs->audio.spider_attack,
                               gs->player.x + gs->player.w / 2.0f, cam_x);
        /* Move birds along their sine-wave patrol paths */
        birds_update(gs->birds, gs->bird_count, dt, gs->audio.flap,
                     gs->player.x + gs->player.w / 2.0f, cam_x);
        faster_birds_update(gs->faster_birds, gs->faster_bird_count, dt, gs->audio.flap,
                            gs->player.x + gs->player.w / 2.0f, cam_x);
        /* Move fish through the water lane and trigger random jump arcs */
        fish_update(gs->fish, gs->fish_count, dt, gs->runtime.world_w);
        /* Move faster fish through the water lane with higher jumps */
        faster_fish_update(gs->faster_fish, gs->faster_fish_count, dt, gs->runtime.world_w);
        /* Advance each spike block along its rail path (cam_x needed for
         * the waiting-until-visible check on fall-off rails) */
        spike_blocks_update(gs->spike_blocks, gs->spike_block_count, dt, cam_x);

        game_float_platforms_update(gs, dt, fp_landed_idx);

        game_bridges_update(gs, dt);

        game_collide(gs, dt);

        game_checkpoint_update(gs);

        /* Advance the water scroll offset (if water is enabled for this level) */
        if (gs->runtime.water_enabled) water_update(&gs->water, dt);
        /* Advance the fog wave positions and spawn the next wave if it is time.
         * Only active when the level definition enables fog. */
        if (gs->runtime.fog_enabled) fog_update(&gs->fog, dt);
        /* Advance the bouncepad release animation (frame 1 → 0 → back to 2) */
        bouncepads_update(gs->bouncepads_medium, gs->bouncepad_medium_count, (Uint32)(dt * 1000.0f));
        bouncepads_update(gs->bouncepads_small, gs->bouncepad_small_count, (Uint32)(dt * 1000.0f));
        bouncepads_update(gs->bouncepads_high, gs->bouncepad_high_count, (Uint32)(dt * 1000.0f));
        /* Advance axe trap swing/spin animation and trigger distance-scaled sound */
        axe_traps_update(gs->axe_traps, gs->axe_trap_count, dt, gs->audio.axe,
                         gs->player.x + gs->player.w / 2.0f, cam_x);
        /* Advance circular saw patrol and spin animation */
        circular_saws_update(gs->circular_saws, gs->circular_saw_count, dt);
        /* Advance blue flame eruption cycles (rise, flip, fall, wait) */
        blue_flames_update(gs->blue_flames, gs->blue_flame_count, dt);
        /* Advance fire flame eruption cycles (same mechanics, fire variant) */
        blue_flames_update(gs->fire_flames, gs->fire_flame_count, dt);

        /* ---- Camera update --------------------------------------- */
        cam_x = game_camera_update(gs, dt);

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

        /*
         * Manual frame cap fallback: if the frame finished before frame_ms ms
         * (e.g. VSync is off), sleep for the remaining time.
         * SDL_Delay sleeps the CPU so we don't burn 100% for no reason.
         */
        /*
         * Manual frame cap fallback (native only — Emscripten controls timing).
         */
#ifndef __EMSCRIPTEN__
        Uint64 elapsed = SDL_GetTicks64() - now;
        if (elapsed < frame_ms) {
            SDL_Delay((Uint32)(frame_ms - elapsed));
        }
#endif

        if (gs->smoke_test_frames > 0) {
            gs->smoke_test_frames--;
            if (gs->smoke_test_frames == 0) {
                gs->running = 0;
            }
        }
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
