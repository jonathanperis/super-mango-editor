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
 * SDL reports axis values in the range [-32768, +32767].  Physical sticks
 * produce small non-zero readings even when untouched (electrical noise,
 * mechanical centre offset).  Ignoring anything below this threshold
 * prevents the player from drifting without touching the controller.
 *
 * 8000 ≈ 24% of the full range — safe for all DualSense / DS4 / Xbox sticks.
 * Raise this value if a specific controller drifts; lower it for more
 * sensitivity at the cost of accidental movement.
 */
#define AXIS_DEAD_ZONE  8000

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
 * We use SDL_GetKeyboardState instead of key-press events because
 * it tells us which keys are held RIGHT NOW, giving smooth, continuous
 * movement rather than one-shot movement on the moment of press.
 */
void player_handle_input(Player *player, Mix_Chunk *snd_jump,
                         SDL_GameController *ctrl,
                         const VineDecor *vines, int vine_count,
                         const LadderDecor *ladders, int ladder_count,
                         const RopeDecor *ropes, int rope_count) {
    /*
     * SDL_GetKeyboardState returns a pointer to an array of key states.
     * Each element is 1 if that key is currently held, 0 if not.
     * Indexed by SDL_SCANCODE_* values (hardware-based, layout-independent).
     * Passing NULL means "use SDL's internal state array".
     */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    int jump_down = keys[SDL_SCANCODE_SPACE] ? 1 : 0;

#ifndef __EMSCRIPTEN__
    if (ctrl && SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A)) {
        jump_down = 1;
    }
#endif

    /*
     * Vine grab — if the player is not already climbing and presses UP
     * while overlapping a vine's grab zone, enter climbing mode.
     *
     * Ignore the grab when jump is held — otherwise holding jump + UP
     * causes the player to grab and immediately jump-dismount every frame,
     * spamming the jump action and accumulating height.
     */
    if (!player->on_vine && !jump_down &&
        (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W])) {
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
        if (keys[SDL_SCANCODE_UP]   || keys[SDL_SCANCODE_W]) player->vy = -CLIMB_SPEED;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) player->vy =  CLIMB_SPEED;

        player->vx = 0.0f;
        if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) {
            player->vx = -CLIMB_H_SPEED;
            player->facing_left = 1;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
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
        player->is_running = (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) ? 1 : 0;
        player->move_dir   = 0;
        if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) {
            player->move_dir    = -1;
            player->facing_left = 1;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
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

    /* ----------------------------------------------------------------
     * Gamepad input — only runs when a controller is connected (ctrl != NULL).
     *
     * SDL_GameController maps every supported device (DualSense, DualShock 4,
     * Xbox Series / One / 360) to a unified button/axis layout regardless of
     * the physical label on the device.  We read two input sources:
     *
     *   1. D-Pad buttons — digital on/off, perfect for precision platforming.
     *   2. Left analog stick X axis — analog range; requires a dead zone check
     *      to filter electrical noise when the stick is at rest.
     *
     * Both sources can be active simultaneously; the velocity accumulates.
     * Keyboard and gamepad also work at the same time — no mode switching.
     *
     * DISABLED ON WEBASSEMBLY: Emscripten's SDL gamepad implementation may
     * report a "virtual" controller with non-zero axis values, causing the
     * player to auto-move or override keyboard input. Skip gamepad entirely
     * on __EMSCRIPTEN__ to ensure keyboard controls work correctly.
     * ---------------------------------------------------------------- */
#ifndef __EMSCRIPTEN__
    if (ctrl) {
        /*
         * Vine grab via D-Pad UP — same logic as keyboard UP above.
         * Skip when A / Cross is held to prevent grab-dismount spam.
         */
        if (!player->on_vine &&
            !jump_down &&
            SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
            player_try_grab_climbable(player, vines, vine_count,
                                      ladders, ladder_count,
                                      ropes, rope_count);
        }

        if (player->on_vine) {
            /* D-Pad vertical climbing */
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP))
                player->vy = -CLIMB_SPEED;
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
                player->vy =  CLIMB_SPEED;

            /* D-Pad horizontal drift (reduced speed) */
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
                player->vx = -CLIMB_H_SPEED;
                player->facing_left = 1;
            }
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
                player->vx = CLIMB_H_SPEED;
                player->facing_left = 0;
            }

            /* Left stick vertical climbing */
            Sint16 axis_y = SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTY);
            if (axis_y < -AXIS_DEAD_ZONE) player->vy = -CLIMB_SPEED;
            else if (axis_y > AXIS_DEAD_ZONE) player->vy = CLIMB_SPEED;

            /* Left stick horizontal drift */
            Sint16 axis_x = SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX);
            if (axis_x < -AXIS_DEAD_ZONE) {
                player->vx = -CLIMB_H_SPEED;
                player->facing_left = 1;
            } else if (axis_x > AXIS_DEAD_ZONE) {
                player->vx = CLIMB_H_SPEED;
                player->facing_left = 0;
            }
        } else {
            /*
             * Normal gamepad controls — D-Pad and analog stick for horizontal
             * movement, A / Cross for jump.
             *
             * Right Bumper (RB / R1) → run, matching the keyboard Shift key.
             * D-Pad and left stick both set move_dir; actual vx acceleration
             * happens in player_update just like the keyboard path.
             */

            /* Right bumper → run mode */
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
                player->is_running = 1;

            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
                player->move_dir    = -1;
                player->facing_left = 1;
            }
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
                player->move_dir    = 1;
                player->facing_left = 0;
            }

            Sint16 axis_x = SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX);
            if (axis_x < -AXIS_DEAD_ZONE) {
                player->move_dir    = -1;
                player->facing_left = 1;
            } else if (axis_x > AXIS_DEAD_ZONE) {
                player->move_dir    = 1;
                player->facing_left = 0;
            }
        }
    }
#endif /* __EMSCRIPTEN__ */
}
