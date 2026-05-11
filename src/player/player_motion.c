/*
 * player_motion.c — Player horizontal acceleration and friction helpers.
 */

#include "player_motion.h"

void player_apply_horizontal_motion(Player *player, float dt, int was_on_ground) {
    /*
     * air_is_running — "snapshot" of is_running taken while on the ground.
     *
     * Every frame the player stands on the ground we refresh this value.
     * The moment they leave (jump or walk off an edge), air_is_running
     * freezes at whatever it was on the last ground frame.
     *
     * This means holding or releasing Shift mid-air has no effect on
     * physics: the momentum built on the ground is what carries the arc.
     */
    if (was_on_ground)
        player->air_is_running = player->is_running;

    /* Ground physics uses live is_running; air uses the frozen snapshot. */
    int running = was_on_ground ? player->is_running : player->air_is_running;

    float max_spd = running ? player->run_max_speed    : player->walk_max_speed;
    float accel   = was_on_ground
                      ? (running ? player->run_ground_accel  : player->walk_ground_accel)
                      : (running ? player->air_accel_run     : player->air_accel_walk);
    float friction = was_on_ground ? player->ground_friction : player->air_friction;

    if (player->move_dir != 0) {
        /*
         * Direction key held — accelerate toward the target speed.
         *
         * Counter-direction bonus: when the pressed direction is opposite
         * to current vx (e.g. moving right but pressing left), the player
         * is actively fighting their own momentum.  On the ground we add
         * ground_counter_accel to brake harder, giving a snappier reversal
         * feel without reducing the passive skid (ground_friction).
         *
         * step = how many px/s to add this frame (accel × dt).
         * We clamp to target_vx so we never overshoot in a single frame.
         */
        int counter = was_on_ground &&
                      ((player->move_dir > 0 && player->vx < 0.0f) ||
                       (player->move_dir < 0 && player->vx > 0.0f));
        float effective_accel = accel + (counter ? player->ground_counter_accel : 0.0f);

        float target_vx = (float)player->move_dir * max_spd;
        float step       = effective_accel * dt;
        if (player->move_dir > 0) {
            /* Moving right: increase vx but don't exceed +target_vx */
            player->vx += step;
            if (player->vx > target_vx) player->vx = target_vx;
        } else {
            /* Moving left: decrease vx but don't go below -target_vx */
            player->vx -= step;
            if (player->vx < target_vx) player->vx = target_vx;
        }
    } else {
        /*
         * No direction key held — friction decelerates vx toward 0.
         * This is what produces the skid on the ground and the gentle
         * float-through in the air.
         *
         * We apply friction symmetrically so it always moves vx toward 0
         * regardless of sign, and clamp to 0 to avoid oscillation.
         */
        float step = friction * dt;
        if (player->vx > 0.0f) {
            player->vx -= step;
            if (player->vx < 0.0f) player->vx = 0.0f;
        } else if (player->vx < 0.0f) {
            player->vx += step;
            if (player->vx > 0.0f) player->vx = 0.0f;
        }
    }
}
