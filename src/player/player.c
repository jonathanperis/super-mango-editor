/*
 * player.c — Implementation of player init, input, physics, rendering, and cleanup.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <SDL_mixer.h>  /* Mix_Chunk, Mix_PlayChannel */
#include <stdio.h>
#include <stdlib.h>

#include "player.h"
#include "../surfaces/bouncepad.h"      /* Bouncepad, BOUNCEPAD_VY — for bouncepad landing collision */
#include "../surfaces/float_platform.h" /* FloatPlatform — for one-way landing collision             */
#include "../surfaces/bridge.h"         /* Bridge — for one-way landing collision on bridges         */
#include "../surfaces/ladder.h"         /* LadderDecor — climbable, same mechanics as vine          */
#include "../surfaces/rope.h"           /* RopeDecor — climbable, same mechanics as vine            */
#include "../game.h"                    /* GAME_W, GAME_H, FLOOR_Y, GRAVITY (physics and clamping)  */

/* ------------------------------------------------------------------ */

/*
 * player_init — Load the sprite and place the player in the center of the window.
 */
/* Width and height of one sprite frame in the sheet (pixels). */
#define FRAME_W 48
#define FRAME_H 48

/*
 * FLOOR_SINK — extra pixels the player overlaps with the top of the floor tile.
 * The sprite sheet has transparent padding at the bottom of each frame, so the
 * physics edge (y + h = FLOOR_Y) leaves the character visually floating.
 * Sinking by this many pixels makes the feet appear to rest on the grass.
 */
#define FLOOR_SINK 16

/*
 * Physics-box insets — how many pixels to trim from each side of the 48×48
 * sprite frame to reach the visible character boundary.
 *
 * The character art is centred in the frame with significant transparent
 * padding, especially on the left and right. Using the raw frame size for
 * collisions creates an invisible wall well outside the art.
 *
 * PHYS_PAD_X : pixels trimmed from the LEFT and RIGHT edges (each side).
 *              Physics width  = FRAME_W − 2 × PHYS_PAD_X  = 48 − 30 = 18 px.
 * PHYS_PAD_TOP : pixels trimmed from the TOP edge.
 *              Combined with FLOOR_SINK at the bottom the physics box
 *              tracks the drawn character art closely on all four sides.
 *
 * Pixel analysis: art occupies x=15..32 (18 px), y=18..33 (16 px).
 */
#define PHYS_PAD_X   15
#define PHYS_PAD_TOP 18

/*
 * Animation tables — indexed by AnimState (0=IDLE, 1=WALK, 2=JUMP, 3=FALL).
 *
 * ANIM_FRAME_COUNT : how many frames the animation has.
 * ANIM_FRAME_MS    : how long each frame is shown, in milliseconds.
 * ANIM_ROW         : which row of the sprite sheet the animation lives on.
 *
 * Frame layout confirmed from Player.png (192×288, 4 cols × 6 rows, 48×48 each):
 *   Row 0 — Idle : 4 frames  (cols 0–3)
 *   Row 1 — Walk : 4 frames  (cols 0–3)
 *   Row 2 — Jump : 2 frames  (cols 0–1, rising-phase poses)
 *   Row 3 — Fall : 1 frame   (col 0, descent pose)
 */
static const int ANIM_FRAME_COUNT[5] = { 4,   4,   2,   1,   2   };
static const int ANIM_FRAME_MS[5]    = { 150, 100, 150, 200, 100 };
static const int ANIM_ROW[5]         = { 0,   1,   2,   3,   4   };

/* ---- Horizontal movement physics (default values) ------------------------
 *
 * All values are in logical pixels and seconds.  These are the engine
 * defaults — each can be overridden per-level via LevelDef.physics in the
 * .toml file (set to 0 there to keep the default).
 *
 * Walk (default, no run key):
 *   WALK_MAX_SPEED    — top speed while walking (px/s).
 *   WALK_GROUND_ACCEL — how quickly the player reaches walk speed on the
 *                       ground (px/s²). Higher = snappier start.
 *
 * Run (Shift on keyboard / Right Bumper on gamepad):
 *   RUN_MAX_SPEED     — top speed while running (px/s).
 *   RUN_GROUND_ACCEL  — ramp-up while running on the ground (px/s²).
 *
 * Friction (shared by both walk and run):
 *   GROUND_FRICTION       — deceleration when no key held on ground (px/s²).
 *   GROUND_COUNTER_ACCEL  — extra brake when pressing opposite direction (px/s²).
 *
 * Air control:
 *   AIR_ACCEL_WALK    — air accel in walk arc (px/s²).
 *   AIR_ACCEL_RUN     — air accel in run arc, less control (px/s²).
 *   AIR_FRICTION      — passive drag in air when no key held (px/s²).
 *
 * Animation:
 *   WALK_ANIM_MIN_VX  — below this speed show idle, not walk (px/s).
 */
#define WALK_MAX_SPEED         100.0f   /* px/s  */
#define RUN_MAX_SPEED          250.0f   /* px/s  */
#define WALK_GROUND_ACCEL      750.0f   /* px/s² */
#define RUN_GROUND_ACCEL       600.0f   /* px/s² */
#define GROUND_FRICTION        550.0f   /* px/s² */
#define GROUND_COUNTER_ACCEL   100.0f   /* px/s² */
#define AIR_ACCEL_WALK         350.0f   /* px/s² */
#define AIR_ACCEL_RUN          180.0f   /* px/s² */
#define AIR_FRICTION            80.0f   /* px/s² */
#define WALK_ANIM_MIN_VX         8.0f   /* px/s  */

void player_init(Player *player, SDL_Renderer *renderer) {
    /*
     * IMG_LoadTexture — decode the PNG sprite sheet and upload it to the GPU.
     * The sheet is 192×288 px and contains a 4-column × 6-row grid of 48×48
     * frames. We only draw one frame at a time using a source clipping rect.
     */
    player->texture = IMG_LoadTexture(renderer, "assets/sprites/player/player.png");
    if (!player->texture) {
        fprintf(stderr, "Failed to load Player.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * frame — the source rectangle: which 48×48 cell to cut from the sheet.
     * {x=0, y=0} → top-left cell, which is the first idle/standing pose.
     * We keep frame.w and frame.h constant at FRAME_W/FRAME_H for now.
     */
    player->frame.x = 0;
    player->frame.y = 0;
    player->frame.w = FRAME_W;
    player->frame.h = FRAME_H;

    /* The on-screen display size matches the frame size exactly. */
    player->w = FRAME_W;
    player->h = FRAME_H;

    /*
     * Place the player horizontally centered, sitting on top of the floor.
     * FLOOR_Y is the top edge of the grass tiles. FLOOR_SINK adds a small
     * downward offset to compensate for transparent padding at the bottom of
     * the sprite frame, so the feet visually rest on the grass surface.
     */
    /*
     * Default spawn on top of the first platform (x=80, y=172, width=TILE_SIZE).
     * Centre horizontally on the pillar; snap vertically using the same
     * formula as platform landing: plat_y − player_h + FLOOR_SINK.
     *
     * spawn_x/spawn_y store the level-defined spawn point.  level_load will
     * override these (and reposition x/y) once the LevelDef is available.
     */
    player->spawn_x  = 80.0f;
    player->spawn_y  = (float)(FLOOR_Y - 2 * TILE_SIZE + 16);
    player->x        = player->spawn_x + (TILE_SIZE - player->w) / 2.0f;
    player->y        = player->spawn_y - player->h + FLOOR_SINK;
    player->vx       = 0.0f;
    player->vy       = 0.0f;   /* start stationary; gravity will pull down   */
    player->speed    = 160.0f; /* horizontal speed: 160 logical px per second */
    player->on_ground = 1;     /* starts on the floor                         */

    /* Animation — start in idle state, first frame, facing right */
    player->anim_state       = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms    = 0;
    player->facing_left      = 0;

    /* Not climbing at startup */
    player->on_vine      = 0;
    player->vine_index   = 0;
    player->climb_source = 0;
    player->jump_held   = 0;
    player->move_dir       = 0;   /* no direction pressed at startup */
    player->is_running     = 0;   /* not running at startup          */
    player->air_is_running = 0;   /* starts on the ground, no run momentum */

    /*
     * Physics fields — initialised from the #define defaults above.
     * level_load may override these after player_init if the LevelDef
     * specifies non-zero physics values for finetuning per level.
     */
    player->walk_max_speed       = WALK_MAX_SPEED;
    player->run_max_speed        = RUN_MAX_SPEED;
    player->walk_ground_accel    = WALK_GROUND_ACCEL;
    player->run_ground_accel     = RUN_GROUND_ACCEL;
    player->ground_friction      = GROUND_FRICTION;
    player->ground_counter_accel = GROUND_COUNTER_ACCEL;
    player->air_accel_walk       = AIR_ACCEL_WALK;
    player->air_accel_run        = AIR_ACCEL_RUN;
    player->air_friction         = AIR_FRICTION;

    /* Not hurt at startup; timer counts down to 0 during invincibility */
    player->hurt_timer = 0.0f;
}

/* ------------------------------------------------------------------ */

/*
 * player_handle_input — Sample the keyboard and set the player's velocity.
 *
 * Called once per frame, before player_update.
 *
 * We use SDL_GetKeyboardState instead of key-press events because
 * it tells us which keys are held RIGHT NOW, giving smooth, continuous
 * movement rather than one-shot movement on the moment of press.
 */
/* ---- Gamepad dead zone --------------------------------------------------- */

/* ---- Gamepad dead zone --------------------------------------------------- */

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
 * Vine climbing constants.
 *
 * CLIMB_SPEED   : vertical speed while climbing, in logical px/s (half of walk).
 * CLIMB_H_SPEED : horizontal drift speed while on vine (half of walk).
 * VINE_GRAB_PAD : extra pixels on each side of the vine sprite that count as
 *                 the "grab zone".  The visual vine is VINE_W (16 px); the
 *                 grab zone is VINE_W + 2 × VINE_GRAB_PAD = 24 px.
 */
#define CLIMB_SPEED     80.0f
#define CLIMB_H_SPEED   80.0f
#define VINE_GRAB_PAD   4

/*
 * vine_grab_rect — Return the axis-aligned grab zone for a vine.
 *
 * The grab zone is wider than the 16 px visual sprite (padded by
 * VINE_GRAB_PAD on each side) and spans the full vine height.
 */
static SDL_Rect vine_grab_rect(const VineDecor *v) {
    int vine_total_h = (v->tile_count - 1) * VINE_STEP + VINE_H;
    return (SDL_Rect){
        (int)v->x - VINE_GRAB_PAD,
        (int)v->y,
        VINE_W + 2 * VINE_GRAB_PAD,
        vine_total_h
    };
}

/*
 * ladder_grab_rect — Return the grab zone for a ladder (same concept as vine).
 */
static SDL_Rect ladder_grab_rect(const LadderDecor *ld) {
    int total_h = (ld->tile_count - 1) * LADDER_STEP + LADDER_H;
    return (SDL_Rect){
        (int)ld->x - VINE_GRAB_PAD,
        (int)ld->y,
        LADDER_W + 2 * VINE_GRAB_PAD,
        total_h
    };
}

/*
 * rope_grab_rect — Return the grab zone for a rope (same concept as vine).
 */
static SDL_Rect rope_grab_rect(const RopeDecor *rp) {
    int total_h = (rp->tile_count - 1) * ROPE_STEP + ROPE_H;
    return (SDL_Rect){
        (int)rp->x - VINE_GRAB_PAD,
        (int)rp->y,
        ROPE_W + 2 * VINE_GRAB_PAD,
        total_h
    };
}

/*
 * climb_get_bounds — Return the grab rect, top y, and bottom y of the
 * currently climbed object, regardless of its type (vine/ladder/rope).
 */
static void climb_get_bounds(const Player *player,
                             const VineDecor *vines,
                             const LadderDecor *ladders,
                             const RopeDecor *ropes,
                             SDL_Rect *out_grab, float *out_top,
                             float *out_bottom) {
    SDL_Rect grab = {0, 0, 0, 0};
    float top = 0.0f, bot = 0.0f;
    int idx = player->vine_index;

    if (player->climb_source == 0) {
        grab = vine_grab_rect(&vines[idx]);
        int vh = (vines[idx].tile_count - 1) * VINE_STEP + VINE_H;
        top = vines[idx].y;
        bot = vines[idx].y + (float)vh;
    } else if (player->climb_source == 1) {
        grab = ladder_grab_rect(&ladders[idx]);
        int lh = (ladders[idx].tile_count - 1) * LADDER_STEP + LADDER_H;
        top = ladders[idx].y;
        bot = ladders[idx].y + (float)lh;
    } else {
        grab = rope_grab_rect(&ropes[idx]);
        int rh = (ropes[idx].tile_count - 1) * ROPE_STEP + ROPE_H;
        top = ropes[idx].y;
        bot = ropes[idx].y + (float)rh;
    }

    *out_grab   = grab;
    *out_top    = top;
    *out_bottom = bot;
}

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

    /*
     * Vine grab — if the player is not already climbing and presses UP
     * while overlapping a vine's grab zone, enter climbing mode.
     *
     * Ignore the grab when Space is held — otherwise holding Space + UP
     * causes the player to grab and immediately jump-dismount every frame,
     * spamming the jump action and accumulating height.
     */
    if (!player->on_vine && !keys[SDL_SCANCODE_SPACE] &&
        (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W])) {
        SDL_Rect phit = player_get_hitbox(player);
        int grabbed = 0;
        /* Check vines */
        for (int i = 0; i < vine_count && !grabbed; i++) {
            SDL_Rect vgrab = vine_grab_rect(&vines[i]);
            if (SDL_HasIntersection(&phit, &vgrab)) {
                player->on_vine      = 1;
                player->vine_index   = i;
                player->climb_source = 0;
                grabbed = 1;
            }
        }
        /* Check ladders */
        for (int i = 0; i < ladder_count && !grabbed; i++) {
            SDL_Rect lgrab = ladder_grab_rect(&ladders[i]);
            if (SDL_HasIntersection(&phit, &lgrab)) {
                player->on_vine      = 1;
                player->vine_index   = i;
                player->climb_source = 1;
                grabbed = 1;
            }
        }
        /* Check ropes */
        for (int i = 0; i < rope_count && !grabbed; i++) {
            SDL_Rect rgrab = rope_grab_rect(&ropes[i]);
            if (SDL_HasIntersection(&phit, &rgrab)) {
                player->on_vine      = 1;
                player->vine_index   = i;
                player->climb_source = 2;
                grabbed = 1;
            }
        }
        if (grabbed) {
            player->on_ground  = 0;
            player->vy         = 0.0f;
            player->vx         = 0.0f;
        }
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
        if (keys[SDL_SCANCODE_SPACE]) {
            player->on_vine   = 0;
            player->vy        = -325.0f;
            player->on_ground = 0;
            if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
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
         * Jump: Space only — requires on_ground AND the key was not already
         * held from a previous jump.  jump_held prevents auto-repeat: the
         * player must release Space and press it again to jump again.
         */
        if (keys[SDL_SCANCODE_SPACE]) {
            if (player->on_ground && !player->jump_held) {
                player->vy         = -325.0f;
                player->on_ground  = 0;
                player->jump_held  = 1;
                if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
            }
        } else {
            player->jump_held = 0;
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
            !SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A) &&
            SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
            SDL_Rect phit = player_get_hitbox(player);
            int grabbed = 0;
            for (int i = 0; i < vine_count && !grabbed; i++) {
                SDL_Rect vgrab = vine_grab_rect(&vines[i]);
                if (SDL_HasIntersection(&phit, &vgrab)) {
                    player->on_vine      = 1;
                    player->vine_index   = i;
                    player->climb_source = 0;
                    grabbed = 1;
                }
            }
            for (int i = 0; i < ladder_count && !grabbed; i++) {
                SDL_Rect lgrab = ladder_grab_rect(&ladders[i]);
                if (SDL_HasIntersection(&phit, &lgrab)) {
                    player->on_vine      = 1;
                    player->vine_index   = i;
                    player->climb_source = 1;
                    grabbed = 1;
                }
            }
            for (int i = 0; i < rope_count && !grabbed; i++) {
                SDL_Rect rgrab = rope_grab_rect(&ropes[i]);
                if (SDL_HasIntersection(&phit, &rgrab)) {
                    player->on_vine      = 1;
                    player->vine_index   = i;
                    player->climb_source = 2;
                    grabbed = 1;
                }
            }
            if (grabbed) {
                player->on_ground  = 0;
                player->vy         = 0.0f;
                player->vx         = 0.0f;
            }
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

            /* A / Cross button → jump dismount */
            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A)) {
                player->on_vine   = 0;
                player->vy        = -500.0f;
                player->on_ground = 0;
                if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
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

            if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A)) {
                if (player->on_ground && !player->jump_held) {
                    player->vy         = -325.0f;
                    player->on_ground  = 0;
                    player->jump_held  = 1;
                    if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
                }
            } else {
                player->jump_held = 0;
            }
        }
    }
#endif /* __EMSCRIPTEN__ */
}

/* ------------------------------------------------------------------ */

/*
 * player_animate — Choose the correct animation state and advance its frame.
 *
 * Called once per frame from player_update, after physics are resolved.
 * Uses the player's velocity and on_ground flag to determine which
 * animation should be playing, then advances the frame timer.
 *
 * On state transitions the frame index is reset to 0 so animations
 * always start from the beginning rather than mid-sequence.
 */
static void player_animate(Player *player, Uint32 dt_ms) {
    /* Determine the target animation from current physics state */
    AnimState target;
    if (player->on_vine) {
        target = ANIM_CLIMB;
    } else if (!player->on_ground) {
        /*
         * In the air: negative vy means moving upward (SDL y-axis is inverted),
         * positive vy means falling down under gravity.
         */
        target = (player->vy < 0.0f) ? ANIM_JUMP : ANIM_FALL;
    } else if (player->vx > WALK_ANIM_MIN_VX || player->vx < -WALK_ANIM_MIN_VX) {
        /*
         * Only show the walk animation above WALK_ANIM_MIN_VX (8 px/s).
         * During a ground skid vx tapers to 0; below the threshold it looks
         * odd to cycle the walk frames for just 1–2 frames, so we show idle.
         */
        target = ANIM_WALK;
    } else {
        target = ANIM_IDLE;
    }

    /* Reset the frame counter whenever the animation state changes */
    if (target != player->anim_state) {
        player->anim_state       = target;
        player->anim_frame_index = 0;
        player->anim_timer_ms    = 0;
    }

    /*
     * Advance the frame timer. When it exceeds the per-frame duration,
     * subtract the duration (rather than resetting to 0) so any leftover
     * time carries into the next frame — keeping animation speed accurate.
     *
     * While climbing with vy == 0 (holding still on the vine), freeze the
     * animation on the current frame so the player looks like they are
     * clinging motionless.
     */
    if (player->anim_state == ANIM_CLIMB && player->vy == 0.0f) {
        /* Freeze — do not advance the timer or frame index */
    } else {
        player->anim_timer_ms += dt_ms;
        Uint32 frame_duration = (Uint32)ANIM_FRAME_MS[player->anim_state];
        if (player->anim_timer_ms >= frame_duration) {
            player->anim_timer_ms -= frame_duration;
            player->anim_frame_index =
                (player->anim_frame_index + 1) % ANIM_FRAME_COUNT[player->anim_state];
        }
    }

    /*
     * Update the source rectangle to point at the correct cell on the sheet.
     *   frame.x = column × FRAME_W  (horizontal offset into the sheet)
     *   frame.y = row    × FRAME_H  (vertical  offset into the sheet)
     */
    player->frame.x = player->anim_frame_index * FRAME_W;
    player->frame.y = ANIM_ROW[player->anim_state] * FRAME_H;
}

/* ------------------------------------------------------------------ */

/*
 * player_update -- Apply gravity and velocity to position, handle floor and
 *                  one-way platform collisions.
 *
 * dt (delta time) is the time in seconds since the last frame (e.g. 0.016).
 * Multiplying velocity (px/s) by dt (s) gives displacement in pixels.
 * This makes movement speed identical regardless of frame rate.
 *
 * One-way platforms:
 *   Only the TOP SURFACE of each platform triggers a landing.  The player
 *   can jump through from below; collision is only checked when the player
 *   is moving downward (vy >= 0) and their bottom edge crosses the platform
 *   top surface this frame (crossing test prevents tunnelling at high speed).
 */
/*
 * FLOAT_PLATFORM_STICK_TOL — tolerance in logical pixels for the stay-on check.
 *
 * When a rail platform moves upward, it escapes from under the player before
 * the crossing test can fire (the player's feet are slightly BELOW the new
 * surface position, but `prev_bottom` was also below the new position so the
 * "from above" test fails).  The stay-on check catches this by accepting any
 * gap smaller than this tolerance between the player's physics bottom and the
 * platform's top surface.
 *
 * Worst-case gap in one frame at the dt cap (0.1 s):
 *   platform moves up : 2 tiles/s × 16 px/tile × 0.1 s = 3.2 px
 *   player falls      : ½ × GRAVITY × dt²            = 4.0 px
 *   total gap                                          = 7.2 px
 * 16 px gives a safe margin over the dt-capped worst case.
 */
#define FLOAT_PLATFORM_STICK_TOL  16

void player_update(Player *player, float dt,
                   const Platform *platforms, int platform_count,
                   const FloatPlatform *float_platforms, int float_platform_count,
                   const Bouncepad *bouncepads, int bouncepad_count,
                   const VineDecor *vines, int vine_count,
                   const LadderDecor *ladders, int ladder_count,
                   const RopeDecor *ropes, int rope_count,
                   const Bridge *bridges, int bridge_count,
                   const SpikePlatform *spike_platforms, int spike_platform_count,
                   const int *floor_gaps, int floor_gap_count,
                   int *out_bounce_idx,
                   int *out_fp_landed_idx,
                   int prev_fp_landed_idx,
                   int world_w) {

    (void)vine_count;    /* vine_index selects the climbable; count unused here */
    (void)ladder_count;
    (void)rope_count;

    /* ---- Vine/ladder/rope climbing physics ----------------------- */
    /*
     * While climbing, gravity and floor/platform collisions are skipped.
     * The player moves only by the velocities set in player_handle_input.
     * Detach if the player drifts horizontally out of the grab zone or
     * their feet drop below the climbable bottom.
     */
    if (player->on_vine) {
        player->x += player->vx * dt;
        player->y += player->vy * dt;

        SDL_Rect grab;
        float climb_top, climb_bottom;
        climb_get_bounds(player, vines, ladders, ropes,
                         &grab, &climb_top, &climb_bottom);
        SDL_Rect phit = player_get_hitbox(player);

        /* Horizontal detach — player drifted out of the grab zone */
        if (!SDL_HasIntersection(&phit, &grab)) {
            player->on_vine = 0;
            player->vy      = 0.0f;
            player_animate(player, (Uint32)(dt * 1000.0f));
            return;
        }

        /* Vertical clamp at climbable top */
        if (player->y + PHYS_PAD_TOP < climb_top) {
            player->y  = climb_top - (float)PHYS_PAD_TOP;
            player->vy = 0.0f;
        }

        /* Bottom detach — feet below climbable bottom → release and fall */
        float player_bottom = player->y + player->h - FLOOR_SINK;
        if (player_bottom > climb_bottom) {
            player->on_vine = 0;
            player->vy      = 0.0f;
        }

        /* Horizontal world clamp (same logic as normal path) */
        if (player->x + PHYS_PAD_X < 0.0f)
            player->x = -(float)PHYS_PAD_X;
        if (player->x + player->w - PHYS_PAD_X > world_w)
            player->x = (float)(world_w - player->w + PHYS_PAD_X);

        player_animate(player, (Uint32)(dt * 1000.0f));
        return;   /* skip normal gravity / floor / platform logic */
    }


    /*
     * Capture the ground state from LAST frame before clearing it.
     * on_ground is about to be reset below; the acceleration block needs the
     * previous value to decide between ground accel and air accel.
     */
    const int was_on_ground = player->on_ground;

    /*
     * Reset on_ground every frame so the player immediately starts falling
     * when they walk off the edge of a platform.  Collision checks below
     * will set it back to 1 if the player is resting on a surface.
     */
    player->on_ground = 0;

    /*
     * Sample the physics bottom BEFORE this frame's movement so the
     * platform crossing test can compare "where was the player last frame"
     * vs "where is the player now".
     *
     * Why capture it here instead of back-projecting later:
     *   back-projection computes  prev = (y_new + 32) - vy_new * dt
     *   which is mathematically   (y_old + vy_new*dt + 32) - vy_new*dt = y_old + 32.
     *   In IEEE 754 single-precision, (a + b) - b can differ from a by up
     *   to 1 ULP due to rounding during the addition.  When the player is
     *   snapped to a platform at y=plat->y, that 1-ULP error makes
     *   prev_bottom > plat->y, the crossing test fails, and the player
     *   silently sinks through the platform every frame until they hit the
     *   floor.  Capturing the true absolute value before the update avoids
     *   all cancellation error.
     */
    const float prev_bottom = player->y + player->h - FLOOR_SINK;
    /*
     * prev_top — sprite top edge before this frame's physics update.
     * Derived from prev_bottom: prev_top = prev_bottom − player->h + FLOOR_SINK.
     * Combined with PHYS_PAD_TOP below to obtain the physical head position
     * for the spike-platform ceiling crossing test.
     */
    const float prev_top    = prev_bottom - player->h + FLOOR_SINK;

    /*
     * Gravity: accelerate downward each frame.
     * Because on_ground was just cleared, gravity always runs here; the
     * floor/platform snap below cancels the tiny fall each frame while the
     * player stands still, keeping the character rock-solid on the ground.
     */
    player->vy += GRAVITY * dt;

    /* ---- Horizontal acceleration / friction (momentum system) ------------ */
    /*
     * player_handle_input sets move_dir (-1/0/+1) and is_running each frame.
     * Here we translate those intentions into a smoothly accelerating vx,
     * rather than snapping instantly to top speed.
     *
     * Ground vs air:
     *   was_on_ground == 1  → use ground accel + strong friction (tight, skiddy).
     *   was_on_ground == 0  → use air accel   + weak   friction  (committed arc).
     *
     * Walk vs run (is_running flag set by Shift / RB):
     *   Walk: moderate max speed, faster accel  → precise, easy to control.
     *   Run:  high    max speed, slower accel   → powerful but takes time to ramp;
     *         air accel is dramatically reduced → mid-air direction changes are hard.
     */
    {
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

    player->x += player->vx * dt;   /* move horizontally */
    player->y += player->vy * dt;   /* move vertically   */

    /*
     * Floor collision — snap to the grass surface.
     *
     * Before zeroing vy and setting on_ground, we check whether the player
     * has landed horizontally over a bouncepad.  If so the bouncepad wins:
     * we apply its launch impulse instead of a normal landing, and leave
     * on_ground = 0 so the player immediately goes airborne.
     *
     * Bouncepads are floor-level objects — their collision is checked here
     * (at FLOOR_Y) rather than as a crossing-test against the pad's visual
     * top edge (FLOOR_Y − BOUNCEPAD_H).  The pad's sprite is decorative;
     * physically it is just a region of the floor that bounces.
     */
    const float ground_snap = (float)(FLOOR_Y - player->h + FLOOR_SINK);

    /*
     * The physics centre of the player determines whether solid ground
     * exists beneath them.  Sea gaps are holes in the floor — the player
     * falls through into the water below.
     */
    float phys_center_x = player->x + player->w / 2.0f;
    int over_ground = 1;
    for (int g = 0; g < floor_gap_count; g++) {
        float gx = (float)floor_gaps[g];
        if (phys_center_x >= gx && phys_center_x < gx + (float)FLOOR_GAP_W) {
            over_ground = 0;
            break;
        }
    }

    if (over_ground && player->y >= ground_snap) {
        player->y = ground_snap;   /* snap to floor in all cases */

        int bounced = 0;
        for (int i = 0; i < bouncepad_count; i++) {
            const Bouncepad *bp = &bouncepads[i];

            /*
             * Horizontal overlap test: use the inset PHYS_PAD_X physics box
             * so only the visible character art overlaps, not transparent padding.
             */
            int h_overlap = (player->x + player->w - PHYS_PAD_X > bp->x + BOUNCEPAD_ART_X) &&
                            (player->x + PHYS_PAD_X < bp->x + BOUNCEPAD_ART_X + BOUNCEPAD_ART_W);
            if (!h_overlap) continue;

            /*
             * The player's physics bottom has reached the floor inside the
             * bouncepad's horizontal zone → launch them upward.
             * BOUNCEPAD_VY (−875 px/s) is 75 % higher than the original
             * −500 px/s jump impulse.
             */
            player->vy        = bouncepads[i].launch_vy;
            player->on_ground = 0;
            *out_bounce_idx   = i;
            bounced           = 1;
            break;   /* first pad wins */
        }

        if (!bounced) {
            /* Normal floor landing: cancel vertical velocity. */
            player->vy        = 0.0f;
            player->on_ground = 1;
        }
    }

    /*
     * One-way platform collision -- top surface only.
     *
     * We only test when:
     *   1. The player is not already on the floor (avoid double-snap).
     *   2. The player is moving downward (vy >= 0), so upward jumps pass through.
     *
     * Crossing test: compare where the player's bottom was BEFORE this frame's
     * movement (prev_bottom, captured above) with where it is NOW (bottom).
     * A landing is detected when the edge crossed the platform's top Y from
     * above to below.  This is frame-rate-independent and handles any fall
     * speed correctly.
     *
     * The "physics bottom" strips the FLOOR_SINK visual offset so contact
     * lands the sprite at the same apparent depth as on the main floor.
     */
    if (!player->on_ground && player->vy >= 0.0f) {
        const float bottom = player->y + player->h - FLOOR_SINK;

        for (int i = 0; i < platform_count; i++) {
            const Platform *plat = &platforms[i];

            /* Horizontal overlap: use the inset physics box, not the full sprite */
            int h_overlap = (player->x + player->w - PHYS_PAD_X > plat->x) &&
                            (player->x + PHYS_PAD_X < plat->x + plat->w);
            if (!h_overlap) continue;

            /* Vertical crossing: bottom was at or above surface, now below */
            if (prev_bottom <= plat->y && bottom >= plat->y) {
                player->y         = plat->y - player->h + FLOOR_SINK;
                player->vy        = 0.0f;
                player->on_ground = 1;
                break;   /* first platform wins */
            }
        }

        /*
         * Float-platform collision — same crossing test as above.
         *
         * Only runs if the player hasn't already landed on a static platform
         * or the floor.  The FLOAT_PLATFORM_H sprite (16 px) is a thin surface
         * so the crossing test is the correct approach: we check whether the
         * player's physics bottom crossed the platform's top surface y this
         * frame, rather than using a distance threshold.
         *
         * When a landing is detected:
         *   • The player is snapped so their physics bottom sits at fp->y.
         *   • vy is zeroed to prevent continued falling.
         *   • on_ground is set so the animation state resolves to IDLE/WALK,
         *     not FALL — this is the main reason the check lives here inside
         *     player_update rather than in game_loop after the fact.
         *   • *out_fp_landed_idx is set to the matching index so game_loop
         *     can drive the crumble timer and nudge the player on rail platforms.
         */
        if (!player->on_ground) {
            for (int i = 0; i < float_platform_count; i++) {
                const FloatPlatform *fp = &float_platforms[i];
                if (!fp->active) continue;

                /* Horizontal overlap using the same inset physics box */
                int h_overlap = (player->x + player->w - PHYS_PAD_X > fp->x) &&
                                (player->x + PHYS_PAD_X < fp->x + fp->w);
                if (!h_overlap) continue;

                /* Vertical crossing: bottom crossed the top surface from above */
                if (prev_bottom <= fp->y && bottom >= fp->y) {
                    player->y          = fp->y - player->h + FLOOR_SINK;
                    player->vy         = 0.0f;
                    player->on_ground  = 1;
                    *out_fp_landed_idx = i;
                    break;   /* first float platform wins */
                }
            }

            /*
             * Stay-on check — handles platforms that moved UPWARD this frame.
             *
             * When the surface escapes upward, the crossing test fails because
             * both prev_bottom and bottom end up BELOW the new fp->y.  We detect
             * this by remembering which platform the player was on last frame
             * (prev_fp_landed_idx) and checking whether the player's physics
             * bottom is still within FLOAT_PLATFORM_STICK_TOL pixels of that
             * surface.  If so, snap back and re-establish contact.
             *
             * The outer `player->vy >= 0` guard already excludes upward jumps,
             * so this check cannot mistakenly re-snap a player who just jumped.
             */
            if (!player->on_ground &&
                prev_fp_landed_idx >= 0 &&
                prev_fp_landed_idx < float_platform_count) {

                const FloatPlatform *sfp = &float_platforms[prev_fp_landed_idx];
                if (sfp->active) {
                    int h_ov = (player->x + player->w - PHYS_PAD_X > sfp->x) &&
                               (player->x + PHYS_PAD_X < sfp->x + sfp->w);
                    /* gap > 0 means player is below the surface (platform rose) */
                    float gap = bottom - sfp->y;
                    if (h_ov && gap >= 0.0f && gap < (float)FLOAT_PLATFORM_STICK_TOL) {
                        player->y          = sfp->y - player->h + FLOOR_SINK;
                        player->vy         = 0.0f;
                        player->on_ground  = 1;
                        *out_fp_landed_idx = prev_fp_landed_idx;
                    }
                }
            }
        }
    }

    /*
     * Bridge collision — same one-way crossing test as static platforms.
     * Only land if the brick under the player's centre is still solid
     * (not already falling or deactivated).
     */
    if (!player->on_ground && player->vy >= 0.0f) {
        const float bottom = player->y + player->h - FLOOR_SINK;
        float pcx = player->x + player->w / 2.0f;

        for (int i = 0; i < bridge_count; i++) {
            const Bridge *br = &bridges[i];

            int h_overlap = (player->x + player->w - PHYS_PAD_X > br->x) &&
                            (player->x + PHYS_PAD_X < br->x + br->brick_count * BRIDGE_TILE_W);
            if (!h_overlap) continue;

            if (!bridge_has_solid_at(br, pcx)) continue;

            if (prev_bottom <= br->base_y && bottom >= br->base_y) {
                player->y         = br->base_y - player->h + FLOOR_SINK;
                player->vy        = 0.0f;
                player->on_ground = 1;
                break;
            }
        }
    }

    /*
     * Spike platform collision — same one-way crossing test as bridges.
     * The player can land on top (solid surface) but will take damage
     * from the spike hitbox check in game.c.
     */
    if (!player->on_ground && player->vy >= 0.0f) {
        const float bottom = player->y + player->h - FLOOR_SINK;
        for (int i = 0; i < spike_platform_count; i++) {
            const SpikePlatform *sp = &spike_platforms[i];
            if (!sp->active) continue;

            int h_overlap = (player->x + player->w - PHYS_PAD_X > sp->x) &&
                            (player->x + PHYS_PAD_X < sp->x + sp->w);
            if (!h_overlap) continue;

            if (prev_bottom <= sp->y && bottom >= sp->y) {
                player->y         = sp->y - player->h + FLOOR_SINK;
                player->vy        = 0.0f;
                player->on_ground = 1;
                break;
            }
        }
    }

    /*
     * Spike platform smooth-underside barrier — blocks upward movement.
     *
     * The top surface (spikes) already handles downward landing above.
     * Here we handle the smooth underside: when the player jumps up and
     * their physical head crosses through the platform bottom, stop them
     * and zero vy, just like hitting a solid ceiling.  No damage is dealt —
     * damage only comes from the spike tips (handled in game.c).
     *
     * We use the PHYSICAL top (player->y + PHYS_PAD_TOP) rather than the
     * raw sprite top so the head snaps flush with the platform underside
     * instead of leaving an 18 px visible gap caused by the transparent
     * top padding in the player sprite.
     *
     * Crossing test (going upward, vy < 0):
     *   prev_phys_top >= sp_bottom  — physical head was at or below underside
     *   curr_phys_top  < sp_bottom  — physical head is now above underside
     */
    if (!player->on_ground && player->vy < 0.0f) {
        const float prev_phys_top = prev_top    + PHYS_PAD_TOP;
        const float curr_phys_top = player->y   + PHYS_PAD_TOP;
        for (int i = 0; i < spike_platform_count; i++) {
            const SpikePlatform *sp = &spike_platforms[i];
            if (!sp->active) continue;

            int h_overlap = (player->x + player->w - PHYS_PAD_X > sp->x) &&
                            (player->x + PHYS_PAD_X < sp->x + sp->w);
            if (!h_overlap) continue;

            /*
             * sp_bottom — the physical underside of the spike platform.
             * SPIKE_PLAT_SRC_H (11 px) is the rendered content height,
             * matching the downward landing check which uses sp->y as top.
             */
            float sp_bottom = sp->y + SPIKE_PLAT_SRC_H;
            if (prev_phys_top >= sp_bottom && curr_phys_top < sp_bottom) {
                /* Snap sprite top so that physical head sits at sp_bottom */
                player->y  = sp_bottom - PHYS_PAD_TOP;
                player->vy = 0.0f;
                break;
            }
        }
    }

    /*
     * Horizontal clamp — keep the PHYSICS body inside the logical canvas.
     * We clamp the inset edge (x + PHYS_PAD_X), not the sprite left edge,
     * so the transparent side-padding can slide off-screen while the visible
     * character stays flush with the border instead of stopping early.
     */
    if (player->x + PHYS_PAD_X < 0.0f)
        player->x = -(float)PHYS_PAD_X;
    if (player->x + player->w - PHYS_PAD_X > world_w)
        player->x = (float)(world_w - player->w + PHYS_PAD_X);

    /*
     * Ceiling clamp — stop upward movement when the physics top hits the
     * canvas ceiling.  PHYS_PAD_TOP lets the transparent head-room of the
     * sprite frame slide above y=0 before the physics edge triggers.
     */
    if (player->y + PHYS_PAD_TOP < 0.0f) {
        player->y  = -(float)PHYS_PAD_TOP;
        player->vy = 0.0f;
    }

    /*
     * Advance the sprite animation based on the resolved physics state.
     * Convert dt (seconds) to milliseconds for the frame timer.
     */
    player_animate(player, (Uint32)(dt * 1000.0f));
}

/* ------------------------------------------------------------------ */

/*
 * player_render — Draw the player sprite at its current position.
 *
 * While hurt_timer > 0 the sprite blinks on/off every 100 ms to give
 * visual feedback that the player was hit and is temporarily invincible.
 */
void player_render(Player *player, SDL_Renderer *renderer, int cam_x) {
    /*
     * Blink effect: during invincibility, convert the remaining hurt time
     * into a 100-ms cadence.  On odd intervals the sprite is hidden,
     * creating a flash / blink pattern.
     */
    if (player->hurt_timer > 0.0f) {
        int interval = (int)(player->hurt_timer * 1000.0f) / 100;
        if (interval % 2 != 0) return;   /* skip this frame — blink off */
    }

    /*
     * dst — where on screen the sprite will appear.
     * x/y are cast from float to int because SDL works in whole pixels.
     * w/h match the frame size (FRAME_W × FRAME_H = 48×48).
     */
    SDL_Rect dst = {
        .x = (int)player->x - cam_x,  /* world → screen: subtract camera offset */
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    /*
     * SDL_RenderCopyEx — like SDL_RenderCopy but supports rotation and flipping.
     * We pass angle=0 and center=NULL (no rotation), and flip the texture
     * horizontally when the player last moved left, so the character always
     * faces the correct direction regardless of which animation is playing.
     */
    SDL_RendererFlip flip = player->facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, player->texture, &player->frame, &dst,
                     0.0, NULL, flip);
}

/* ------------------------------------------------------------------ */

/*
 * player_get_hitbox — Return the player's physics hitbox as an SDL_Rect.
 *
 * The hitbox is inset from the full 48×48 sprite frame on every side
 * to match the visible character art.  Used by game.c for collision
 * checks against enemies.
 */
SDL_Rect player_get_hitbox(const Player *player) {
    SDL_Rect r;
    r.x = (int)(player->x) + PHYS_PAD_X;
    r.y = (int)(player->y) + PHYS_PAD_TOP;
    r.w = player->w - 2 * PHYS_PAD_X;
    r.h = player->h - PHYS_PAD_TOP - FLOOR_SINK;
    return r;
}

/* ------------------------------------------------------------------ */

/*
 * player_reset — Reset position, velocity, and animation to the initial
 *                state without reloading the texture from disk.
 *
 * Called when the player loses all hearts and a life is consumed.
 * The texture and display dimensions (w/h/speed) are left untouched
 * because they were set once in player_init and don't change.
 */
void player_reset(Player *player) {
    /* Respawn at the level-defined spawn point (set by level_load). */
    player->x         = player->spawn_x + (TILE_SIZE - player->w) / 2.0f;
    player->y         = player->spawn_y - player->h + FLOOR_SINK;
    player->vx        = 0.0f;
    player->vy        = 0.0f;
    player->on_ground = 1;

    player->anim_state       = ANIM_IDLE;
    player->anim_frame_index = 0;
    player->anim_timer_ms    = 0;
    player->facing_left      = 0;
    player->on_vine          = 0;
    player->vine_index       = 0;
    player->climb_source     = 0;
    player->jump_held        = 0;
    player->move_dir         = 0;
    player->is_running       = 0;
    player->air_is_running   = 0;
    player->hurt_timer       = 0.0f;

    player->frame.x = 0;
    player->frame.y = 0;
}

/* ------------------------------------------------------------------ */

/*
 * player_cleanup — Release GPU memory held by the player's texture.
 *
 * Must be called before the renderer is destroyed, because SDL_Texture
 * objects are owned by the renderer that created them.
 */
void player_cleanup(Player *player) {
    if (player->texture) {
        SDL_DestroyTexture(player->texture);
        player->texture = NULL;   /* guard against accidental double-free */
    }
}
