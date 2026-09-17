/*
 * game_loop.c — Main frame loop and per-frame dispatch.
 */

#include "../game.h"

#include "game_overlay.h"
#include "game_timing.h"
#include "game_update.h"
#include "game_inspector.h"
#include "../input/game_events.h"
#include "../input/game_input.h"
#include "../input/game_replay.h"
#include "../input/game_web_input.h"
#include "../render/game_render.h"
#include "../screens/settings_menu.h"

/* Execute one frame. AppSession is the only cross-platform loop owner. */
int game_frame(GameState *gs)
{
    const Uint32 frame_ms = 1000 / TARGET_FPS;
    Uint64 frame_start_ticks = 0;
    float dt = game_timing_step(gs, &frame_start_ticks);

    /* AppSession publishes readiness; game code only discovers an open handle. */
    if (!gs->controller_init_pending) gamepad_refresh_controller(gs);

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
    float simulation_dt = game_inspector_step(gs, dt);
    if (simulation_dt > 0) {
        cam_x = game_update_active(gs, simulation_dt, cam_x);
    } else {
        /* Do not replay taps made while a pause/settings/terminal screen owns
         * input. Still-held movement remains available when resuming. */
        (void)game_web_input_take_touch_mask();
    }

    /* ---- 3. Render ----------------------------------------------- */
    int presented = game_render_frame(gs, cam_x, dt);

    if (gs->smoke_test_frames == 0) game_timing_cap_frame(frame_start_ticks, frame_ms);
    game_timing_tick_smoke(gs);
    return presented;
}

/*
 * game_loop — Legacy direct native helper. The application uses AppSession so
 * menu and game share one callback and one frame owner.
 */
void game_loop(GameState *gs)
{
    gs->loop.prev_ticks = SDL_GetTicks64();
    gs->loop.fp_prev_riding = -1;
    while (gs->running && gs->route == GAME_ROUTE_NONE) {
        game_frame(gs);
    }
}
