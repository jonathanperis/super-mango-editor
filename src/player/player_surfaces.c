/*
 * player_surfaces.c — Player surface collision helpers.
 */

#include "player_surfaces.h"

#include "player_internal.h"
#include "../game_constants.h" /* FLOOR_Y, FLOOR_GAP_W */

/*
 * FLOAT_PLATFORM_STICK_TOL — tolerance in logical pixels for the stay-on check.
 *
 * After every step a rail platform carries its rider by exactly the distance
 * it moved (game_float_platforms.c), so the rider starts the next step with
 * their feet on the surface. If the platform was moving DOWN, those feet are
 * now below where the top was before that move (fp->prev_y), so the crossing
 * test below, which wants the feet to start at or above prev_y, fails. The
 * stay-on check catches this by accepting any gap smaller than this tolerance
 * between the rider's physics bottom and the platform's top surface.
 *
 * In one fixed step (dt = 1/60 s for live play and replays alike) the rider
 * only falls by gravity, GRAVITY × dt² ≈ 0.2 px from vy = 0, whatever the
 * platform's speed. 16 px is a generous margin over that.
 */
#define FLOAT_PLATFORM_STICK_TOL  16

void player_resolve_floor_collision(Player *player,
                                    const BouncepadList *bouncepad_lists, int bouncepad_list_count,
                                    const int *floor_gaps, int floor_gap_count,
                                    float prev_center_x, float prev_bottom,
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
     * Gap walls — once the player's feet are below the floor surface inside
     * a gap, the gap's sides are solid. Without this, a player who had
     * already sunk into the hole could steer sideways until their centre
     * was over grass again, and the snap below would lift them back up onto
     * the floor from inside it.
     *
     * "Inside" is decided from where the player was BEFORE this step: feet
     * below the floor top (prev_bottom > FLOOR_Y) with the centre over a
     * gap. Then the centre is held between that gap's edges; the right edge
     * is kept one pixel in, because a centre exactly on gx + FLOOR_GAP_W
     * counts as ground.
     */
    if (prev_bottom > (float)FLOOR_Y) {
        for (int g = 0; g < floor_gap_count; g++) {
            float gx = (float)floor_gaps[g];
            if (prev_center_x < gx || prev_center_x >= gx + (float)FLOOR_GAP_W)
                continue;
            float center_x = player->x + player->w / 2.0f;
            float wall_left = gx;
            float wall_right = gx + (float)FLOOR_GAP_W - 1.0f;
            if (center_x < wall_left || center_x > wall_right) {
                float held = center_x < wall_left ? wall_left : wall_right;
                player->x  = held - player->w / 2.0f;
                player->vx = 0.0f;
            }
            break;
        }
    }

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
        int flat_index = 0;   /* counts pads across all lists, in list order */
        for (int l = 0; l < bouncepad_list_count && !bounced; l++) {
            const BouncepadList *list = &bouncepad_lists[l];
            for (int i = 0; i < list->count; i++, flat_index++) {
                const Bouncepad *bp = &list->pads[i];

                /*
                 * Horizontal overlap test: use the inset PHYS_PAD_X physics box
                 * so only the visible character art overlaps, not transparent padding.
                 */
                int h_overlap = (player->x + player->w - PHYS_PAD_X > bp->x + BOUNCEPAD_ART_X) &&
                                (player->x + PHYS_PAD_X < bp->x + BOUNCEPAD_ART_X + BOUNCEPAD_ART_W);
                if (!h_overlap) continue;

                /*
                 * The player's physics bottom has reached the floor inside the
                 * bouncepad's horizontal zone → launch them upward with the
                 * pad's own launch_vy. on_ground stays 0, so the player can
                 * never jump from a pad; that is why the level validator
                 * requires every pad to launch at least as hard as JUMP_VY.
                 */
                player->vy        = bp->launch_vy;
                player->on_ground = 0;
                *out_bounce_idx   = flat_index;
                bounced           = 1;
                break;   /* first pad wins */
            }
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
    *out_fp_landed_idx = -1;

    /*
     * One-way platform collision -- top surface only.
     *
     * We only test when:
     * The player is moving downward (vy >= 0), so upward jumps pass through.
     * Each candidate shortens the remaining fall to the nearest surface.
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
    if (player->vy < 0.0f) {
        return;
    }

    float bottom = player->y + player->h - FLOOR_SINK;

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
            bottom = plat->y;  /* later candidates must be nearer, never lower */
        }
    }

    /*
     * Float-platform collision — a crossing test measured against the
     * platform, because the platform moves too.
     *
     * A float platform can replace a lower static surface candidate.
     * The FLOAT_PLATFORM_H sprite (16 px) is a thin surface
     * so the crossing test is the correct approach: we check whether the
     * player's physics bottom crossed the platform's top surface y this
     * frame, rather than using a distance threshold.
     *
     * prev_bottom was where the feet were when the platform's top was still
     * at fp->prev_y: the player moved during the last step BEFORE the
     * platforms did (game_update.c). So "the feet were above the top and are
     * now at or below it" is  prev_bottom <= fp->prev_y && bottom >= fp->y.
     * Comparing prev_bottom with the new fp->y instead would let a platform
     * that rose past falling feet during its own update slip under them.
     *
     * When a landing is detected:
     *   • The player is snapped so their physics bottom sits at fp->y.
     *   • vy is zeroed to prevent continued falling.
     *   • on_ground is set so the animation state resolves to IDLE/WALK,
     *     not FALL — this is the main reason the check lives here inside
     *     player_update rather than in the frame update after the fact.
     *   • *out_fp_landed_idx is set to the matching index so the frame update
     *     can drive the crumble timer and nudge the player on rail platforms.
     */
    {
        for (int i = 0; i < float_platform_count; i++) {
            const FloatPlatform *fp = &float_platforms[i];
            if (!fp->active) continue;

            /* Horizontal overlap using the same inset physics box. */
            int h_overlap = (player->x + player->w - PHYS_PAD_X > fp->x) &&
                            (player->x + PHYS_PAD_X < fp->x + fp->w);
            if (!h_overlap) continue;

            /* Vertical crossing, relative to the moving top surface. */
            if (prev_bottom <= fp->prev_y && bottom >= fp->y) {
                player->y          = fp->y - player->h + FLOOR_SINK;
                player->vy         = 0.0f;
                player->on_ground  = 1;
                *out_fp_landed_idx = i;
                bottom = fp->y;
            }
        }

        /*
         * Stay-on check — keeps a rider on a platform that moved DOWNWARD.
         *
         * The rider was carried down with the platform, so prev_bottom is
         * below fp->prev_y and the crossing test fails, though the feet are
         * still on the surface.  We detect this by remembering which platform
         * the player was on last frame (prev_fp_landed_idx) and checking
         * whether the player's physics bottom is still within
         * FLOAT_PLATFORM_STICK_TOL pixels of that surface.  If so, snap back
         * and re-establish contact.
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

void player_resolve_bridge_collision(Player *player,
                                     const Bridge *bridges, int bridge_count,
                                     float prev_bottom,
                                     int *out_bridge_landed_idx) {
    /*
     * Bridge collision — same one-way crossing test as static platforms.
     * Only land if the brick under the player's centre is still solid
     * (not already falling or deactivated).
     *
     * The index of the bridge landed on is reported back, the way float
     * platforms and bouncepads report theirs, so the bridge update starts
     * crumbling exactly the bridge this test landed on, and nothing else.
     */
    *out_bridge_landed_idx = -1;
    if (player->vy < 0.0f) {
        return;
    }

    float bottom = player->y + player->h - FLOOR_SINK;
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
            *out_bridge_landed_idx = i;
            bottom = br->base_y;
        }
    }
}

void player_resolve_spike_platform_top_collision(Player *player,
                                                 const SpikePlatform *spike_platforms,
                                                 int spike_platform_count,
                                                 float prev_bottom) {
    /*
     * Spike platform collision — same one-way crossing test as bridges.
     * The player can land on top (solid surface) but will take damage
     * from the spike hitbox check in game_collision.c.
     */
    if (player->vy < 0.0f) {
        return;
    }

    float bottom = player->y + player->h - FLOOR_SINK;
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
            bottom = sp->y;
        }
    }
}

void player_resolve_spike_platform_ceiling_collision(Player *player,
                                                     const SpikePlatform *spike_platforms,
                                                     int spike_platform_count,
                                                     float prev_top) {
    /*
     * Spike platform smooth-underside barrier — blocks upward movement.
     *
     * The top surface (spikes) already handles downward landing above.
     * Here we handle the smooth underside: when the player jumps up and
     * their physical head crosses through the platform bottom, stop them
     * and zero vy, just like hitting a solid ceiling.  No damage is dealt —
     * damage only comes from the spike tips (handled in game_collision.c).
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
    if (player->on_ground || player->vy >= 0.0f) {
        return;
    }

    const float prev_phys_top = prev_top  + PHYS_PAD_TOP;
    float curr_phys_top = player->y + PHYS_PAD_TOP;
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
            /* Snap sprite top so that physical head sits at sp_bottom. */
            player->y  = sp_bottom - PHYS_PAD_TOP;
            player->vy = 0.0f;
            curr_phys_top = sp_bottom;
        }
    }
}

void player_resolve_world_bounds(Player *player, int world_w) {
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
}
