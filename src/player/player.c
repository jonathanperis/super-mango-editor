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
