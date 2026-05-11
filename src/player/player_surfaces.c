/*
 * player_surfaces.c — Player surface collision helpers.
 */

#include "player_surfaces.h"

#include "player_internal.h"
#include "../game.h"  /* FLOOR_Y, FLOOR_GAP_W */

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

void player_resolve_floor_collision(Player *player,
                                    const Bouncepad *bouncepads, int bouncepad_count,
                                    const int *floor_gaps, int floor_gap_count,
                                    int *out_bounce_idx) {
    *out_bounce_idx = -1;

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
}

void player_resolve_platform_collisions(Player *player,
                                        const Platform *platforms, int platform_count,
                                        const FloatPlatform *float_platforms,
                                        int float_platform_count,
                                        float prev_bottom,
                                        int *out_fp_landed_idx,
                                        int prev_fp_landed_idx) {
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
    if (player->on_ground || player->vy < 0.0f) {
        return;
    }

    const float bottom = player->y + player->h - FLOOR_SINK;

    for (int i = 0; i < platform_count; i++) {
        const Platform *plat = &platforms[i];

        /* Horizontal overlap: use the inset physics box, not the full sprite. */
        int h_overlap = (player->x + player->w - PHYS_PAD_X > plat->x) &&
                        (player->x + PHYS_PAD_X < plat->x + plat->w);
        if (!h_overlap) continue;

        /* Vertical crossing: bottom was at or above surface, now below. */
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

            /* Horizontal overlap using the same inset physics box. */
            int h_overlap = (player->x + player->w - PHYS_PAD_X > fp->x) &&
                            (player->x + PHYS_PAD_X < fp->x + fp->w);
            if (!h_overlap) continue;

            /* Vertical crossing: bottom crossed the top surface from above. */
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
                /* gap > 0 means player is below the surface (platform rose). */
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
