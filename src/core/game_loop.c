/*
 * game_loop.c — Main frame loop and per-frame dispatch.
 *
 * One call to game_frame renders one picture, but the simulation inside it
 * advances in fixed steps (game_timing.h explains why). On a 60 Hz display
 * that is one step per frame; on 144 Hz most frames run zero steps and only
 * redraw; after a slow frame several steps run before the next picture.
 */

#include "../game.h"

#include "game_experiment.h"
#include "game_overlay.h"
#include "game_timing.h"
#include "game_update.h"
#include "game_inspector.h"
#include "../input/game_events.h"
#include "../input/game_input.h"
#include "../input/game_replay.h"
#include "../input/game_web_input.h"
#include "../render/game_render.h"

/* Execute one frame. AppSession is the only cross-platform loop owner. */
int game_frame(GameState *gs)
{
    /* ---- 1. Time ------------------------------------------------- */
    /* Real seconds since the last frame (clamped); smoke/replay runs get
     * exactly one fixed step so they are identical on every machine. */
    float frame_seconds = game_timing_frame_seconds(gs);

    gamepad_refresh_controller(gs);

    /* ---- 2. Events ----------------------------------------------- */
    game_replay_inject_events(gs);
    game_handle_events(gs);

    /*
     * cam_x stays in scope for paused and active paths. Paused frames render
     * with the previous camera position; active frames replace it with the
     * smoothed camera result from game_update_active().
     */
    int cam_x = (int)gs->camera.x;

    /* ---- 3. Fixed-step simulation -------------------------------- */
    /*
     * While a pause/settings/terminal screen owns input, drop its touch taps
     * so they are not replayed into gameplay later; still-held movement is
     * sampled again on resume. A frame that merely runs zero steps (a fast
     * display between two steps) keeps its taps for the next step.
     */
    if (game_simulation_blocked(gs)) (void)game_web_input_take_touch_mask();

    int steps = game_inspector_steps(gs, frame_seconds);
    for (int i = 0; i < steps; i++) {
        /* Every step, live or replayed, is GAME_FIXED_STEP; a replayed
         * experiment returns 0 once its tape has ended. */
        float step_dt = game_experiment_dt(gs, GAME_FIXED_STEP);
        if (step_dt <= 0.0f) break;
        cam_x = game_update_active(gs, step_dt, cam_x);
        /* Completion, game over or a route stops the remaining steps. */
        if (game_simulation_blocked(gs)) break;
    }

    /* ---- 4. Render ----------------------------------------------- */
    /* Rendering still runs on zero-step and blocked frames, so the last
     * picture stays on screen. Render-only timers use real frame time. */
    int presented = game_render_frame(gs, cam_x, frame_seconds);

    game_timing_tick_smoke(gs);
    return presented;
}
