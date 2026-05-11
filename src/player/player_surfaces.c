/*
 * player_surfaces.c — Player surface collision helpers.
 */

#include "player_surfaces.h"

#include "player_internal.h"
#include "../game.h"  /* FLOOR_Y, FLOOR_GAP_W */

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
