/*
 * game_loop.c — Main frame loop and per-frame dispatch.
 */

#include "../game.h"

#include "game_overlay.h"
#include "game_timing.h"
#include "game_update.h"
#include "../input/game_events.h"
#include "../input/game_input.h"
#include "../input/game_replay.h"
#include "../input/game_web_input.h"
#include "../render/game_render.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/*
 * game_loop_frame — Execute one frame of the game loop.
 *
 * The void* signature matches emscripten_set_main_loop_arg. Native builds pass
 * the same GameState pointer from a normal while loop, so both platforms share
 * identical event, update, render, and frame-timing behavior.
 */
static void game_loop_frame(void *arg)
{
    GameState *gs = (GameState *)arg;
    const Uint32 frame_ms = 1000 / TARGET_FPS;
    Uint64 frame_start_ticks = 0;
    float dt = game_timing_step(gs, &frame_start_ticks);

    /* ---- 1. Events ----------------------------------------------- */
    game_replay_inject_events(gs);
    game_handle_events(gs);

    /*
     * cam_x stays in scope for paused and active paths. Paused frames render
     * with the previous camera position; active frames replace it with the
     * smoothed camera result from game_update_active().
     */
    int cam_x = (int)gs->camera.x;

    /*
     * Skip physics and game logic while an overlay is showing. Rendering still
     * runs so the last visible frame remains on screen and in OS thumbnails.
     */
    if (!game_overlay_blocks_update(gs)) {
        cam_x = game_update_active(gs, dt, cam_x);
    }

    /* ---- 3. Render ----------------------------------------------- */
    game_render_frame(gs, cam_x, dt);

    /*
     * Deferred gamepad init runs after the first presented frame. Native builds
     * may spawn/check the background SDL_INIT_GAMECONTROLLER thread here;
     * WebAssembly builds compile this path as a no-op inside game_input.c.
     */
    gamepad_update_deferred_init(gs);

    game_timing_cap_frame(frame_start_ticks, frame_ms);
    game_timing_tick_smoke(gs);
}

/*
 * game_loop — Run frames until shutdown.
 *
 * Native builds block in a while loop. Emscripten hands frame execution to the
 * browser through requestAnimationFrame via emscripten_set_main_loop_arg.
 */
void game_loop(GameState *gs)
{
    gs->loop.prev_ticks = SDL_GetTicks64();
    gs->loop.fp_prev_riding = -1;

#ifdef __EMSCRIPTEN__
    game_web_input_flush_stale_keys();

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
