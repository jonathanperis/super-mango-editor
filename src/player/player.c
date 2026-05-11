/*
 * player.c — Implementation of player init and physics update.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <SDL_mixer.h>  /* Mix_Chunk */
#include <stdio.h>
#include <stdlib.h>

#include "player.h"
#include "player_animation.h"
#include "player_climb.h"
#include "player_internal.h"
#include "player_jump.h"
#include "player_motion.h"
#include "player_surfaces.h"
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
     * We keep frame.w and frame.h constant at PLAYER_FRAME_W/H for now.
     */
    player->frame.x = 0;
    player->frame.y = 0;
    player->frame.w = PLAYER_FRAME_W;
    player->frame.h = PLAYER_FRAME_H;

    /* The on-screen display size matches the frame size exactly. */
    player->w = PLAYER_FRAME_W;
    player->h = PLAYER_FRAME_H;

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
    player->y        = player->spawn_y - player->h + PLAYER_FLOOR_SINK;
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
    player->coyote_timer = PLAYER_COYOTE_TIME;
    player->jump_buffer_timer = 0.0f;
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
void player_update(Player *player, float dt, Mix_Chunk *snd_jump,
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

    if (player_update_climbing(player, dt, vines, ladders, ropes, world_w)) {
        return;
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

    player_apply_horizontal_motion(player, dt, was_on_ground);

    player->x += player->vx * dt;   /* move horizontally */
    player->y += player->vy * dt;   /* move vertically   */

    player_resolve_floor_collision(player, bouncepads, bouncepad_count,
                                   floor_gaps, floor_gap_count,
                                   out_bounce_idx);

    player_resolve_platform_collisions(player, platforms, platform_count,
                                       float_platforms, float_platform_count,
                                       prev_bottom, out_fp_landed_idx,
                                       prev_fp_landed_idx);

    player_resolve_bridge_collision(player, bridges, bridge_count, prev_bottom);
    player_resolve_spike_platform_top_collision(player, spike_platforms,
                                                spike_platform_count,
                                                prev_bottom);
    player_resolve_spike_platform_ceiling_collision(player, spike_platforms,
                                                    spike_platform_count,
                                                    prev_top);

    player_resolve_world_bounds(player, world_w);

    if (player->on_ground) {
        player->coyote_timer = COYOTE_TIME;
    } else if (was_on_ground && player->vy >= 0.0f) {
        player->coyote_timer = COYOTE_TIME;
    } else if (player->coyote_timer > 0.0f) {
        player->coyote_timer -= dt;
        if (player->coyote_timer < 0.0f) player->coyote_timer = 0.0f;
    }

    const int landed_this_frame = !was_on_ground && player->on_ground;
    if (player->jump_buffer_timer > 0.0f && landed_this_frame) {
        player_start_jump(player, snd_jump);
    } else if (player->jump_buffer_timer > 0.0f) {
        player->jump_buffer_timer -= dt;
        if (player->jump_buffer_timer < 0.0f) player->jump_buffer_timer = 0.0f;
    }

    /*
     * Advance the sprite animation based on the resolved physics state.
     * Convert dt (seconds) to milliseconds for the frame timer.
     */
    player_animate(player, (Uint32)(dt * 1000.0f));
}
