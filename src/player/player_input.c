/*
 * player_input.c — Player keyboard and gamepad input sampling.
 */

#include "player.h"

#include "player_climb.h"
#include "player_jump.h"

/*
 * AXIS_DEAD_ZONE — minimum absolute value an analog axis must exceed before
 * it is treated as intentional input.
 *
 * Saved dead-zone units span [-32768, +32767]. Physical sticks
 * produce small non-zero readings even when untouched (electrical noise,
 * mechanical centre offset).  Ignoring anything below this threshold
 * prevents the player from drifting without touching the controller.
 *
 * 8000 ≈ 24% of the full range — safe for all DualSense / DS4 / Xbox sticks.
 * Raise this value if a specific controller drifts; lower it for more
 * sensitivity at the cost of accidental movement.
 */
/*
 * Climbing movement constants.
 *
 * CLIMB_SPEED   : vertical speed while climbing, in logical px/s (half of walk).
 * CLIMB_H_SPEED : horizontal drift speed while on vine (half of walk).
 */
#define CLIMB_SPEED     80.0f
#define CLIMB_H_SPEED   80.0f

/*
 * player_handle_input — Sample the keyboard and set the player's velocity.
 *
 * Called once per frame, before player_update.
 *
 * Physical input arrives as a sampled mask. The game input module owns device
 * state and route-release gating; this module only applies gameplay controls.
 */
void player_handle_input(Player *player, SoundEffect *snd_jump,
                         unsigned int replay_input_mask,
                         unsigned int physical_input_mask,
                         const VineDecor *vines, int vine_count,
                         const LadderDecor *ladders, int ladder_count,
                         const RopeDecor *ropes, int rope_count) {
    const int replay_left = (replay_input_mask & PLAYER_INPUT_LEFT) != 0;
    const int replay_right = (replay_input_mask & PLAYER_INPUT_RIGHT) != 0;
    const int replay_up = (replay_input_mask & PLAYER_INPUT_UP) != 0;
    const int replay_down = (replay_input_mask & PLAYER_INPUT_DOWN) != 0;
    const int replay_jump = (replay_input_mask & PLAYER_INPUT_JUMP) != 0;
    const int replay_run = (replay_input_mask & PLAYER_INPUT_RUN) != 0;
    const int physical_left = (physical_input_mask & PLAYER_INPUT_LEFT) != 0;
    const int physical_right = (physical_input_mask & PLAYER_INPUT_RIGHT) != 0;
    const int physical_up = (physical_input_mask & PLAYER_INPUT_UP) != 0;
    const int physical_down = (physical_input_mask & PLAYER_INPUT_DOWN) != 0;
    const int physical_jump = (physical_input_mask & PLAYER_INPUT_JUMP) != 0;
    const int physical_run = (physical_input_mask & PLAYER_INPUT_RUN) != 0;
    int jump_down = (physical_jump || replay_jump) ? 1 : 0;

    /*
     * Vine grab — if the player is not already climbing and presses UP
     * while overlapping a vine's grab zone, enter climbing mode.
     *
     * Ignore the grab when jump is held — otherwise holding jump + UP
     * causes the player to grab and immediately jump-dismount every frame,
     * spamming the jump action and accumulating height.
     */
    if (!player->on_vine && !jump_down &&
        (physical_up || replay_up)) {
        player_try_grab_climbable(player, vines, vine_count,
                                  ladders, ladder_count,
                                  ropes, rope_count);
    }

    if (player->on_vine) {
        /*
         * Climbing controls — vertical movement along the vine, reduced
         * horizontal drift, and Space to jump-dismount.
         */
        player->vy = 0.0f;
        if (physical_up || replay_up) player->vy = -CLIMB_SPEED;
        if (physical_down || replay_down) player->vy =  CLIMB_SPEED;

        player->vx = 0.0f;
        if (physical_left || replay_left) {
            player->vx = -CLIMB_H_SPEED;
            player->facing_left = 1;
        }
        if (physical_right || replay_right) {
            player->vx = CLIMB_H_SPEED;
            player->facing_left = 0;
        }

        /* Jump dismount — leap off the vine with a normal upward impulse */
        if (jump_down) {
            player->on_vine   = 0;
            player_start_jump(player, snd_jump);
        }
    } else {
        /*
         * Normal ground controls — record direction intent and run state.
         *
         * We no longer set vx directly here.  Instead we store move_dir
         * (-1 / 0 / +1) and is_running, then player_update applies
         * acceleration and friction to smoothly ramp vx toward the target
         * speed.  This produces:
         *   • A ramp-up feel on direction change (not instant top speed).
         *   • A skid-to-stop on the ground when the key is released.
         *   • Committed jump arcs: air control is weaker once airborne.
         *
         * Run key: Left or Right Shift → higher max speed, less air control.
         */
        player->is_running = (physical_run || replay_run) ? 1 : 0;
        player->move_dir   = 0;
        if (physical_left || replay_left) {
            player->move_dir    = -1;
            player->facing_left = 1;
        }
        if (physical_right || replay_right) {
            player->move_dir    = 1;
            player->facing_left = 0;
        }

        /*
         * Jump: Space or A/Cross — fresh presses are buffered briefly so a
         * press just before landing still jumps on contact. Releasing all jump
         * inputs during upward motion cuts the jump short for controllable hop
         * height.
         */
        if (jump_down) {
            if (!player->jump_held) {
                player_press_jump(player, snd_jump);
            }
        } else {
            player_release_jump(player);
        }
    }

}
