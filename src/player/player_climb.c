/*
 * player_climb.c — Grab-zone helpers for vine/ladder/rope climbing.
 */

#include "player_climb.h"

/* Extra pixels on each side of the climbable sprite that count as grabbable. */
#define PLAYER_CLIMB_GRAB_PAD 4

/*
 * player_vine_grab_rect — Return the axis-aligned grab zone for a vine.
 *
 * The grab zone is wider than the 16 px visual sprite (padded by
 * PLAYER_CLIMB_GRAB_PAD on each side) and spans the full vine height.
 */
SDL_Rect player_vine_grab_rect(const VineDecor *v) {
    int vine_total_h = (v->tile_count - 1) * VINE_STEP + VINE_H;
    return (SDL_Rect){
        (int)v->x - PLAYER_CLIMB_GRAB_PAD,
        (int)v->y,
        VINE_W + 2 * PLAYER_CLIMB_GRAB_PAD,
        vine_total_h
    };
}

/*
 * player_ladder_grab_rect — Return the grab zone for a ladder.
 */
SDL_Rect player_ladder_grab_rect(const LadderDecor *ld) {
    int total_h = (ld->tile_count - 1) * LADDER_STEP + LADDER_H;
    return (SDL_Rect){
        (int)ld->x - PLAYER_CLIMB_GRAB_PAD,
        (int)ld->y,
        LADDER_W + 2 * PLAYER_CLIMB_GRAB_PAD,
        total_h
    };
}

/*
 * player_rope_grab_rect — Return the grab zone for a rope.
 */
SDL_Rect player_rope_grab_rect(const RopeDecor *rp) {
    int total_h = (rp->tile_count - 1) * ROPE_STEP + ROPE_H;
    return (SDL_Rect){
        (int)rp->x - PLAYER_CLIMB_GRAB_PAD,
        (int)rp->y,
        ROPE_W + 2 * PLAYER_CLIMB_GRAB_PAD,
        total_h
    };
}

/*
 * player_climb_get_bounds — Return the grab rect, top y, and bottom y of the
 * currently climbed object, regardless of its type (vine/ladder/rope).
 */
void player_climb_get_bounds(const Player *player,
                             const VineDecor *vines,
                             const LadderDecor *ladders,
                             const RopeDecor *ropes,
                             SDL_Rect *out_grab, float *out_top,
                             float *out_bottom) {
    SDL_Rect grab = {0, 0, 0, 0};
    float top = 0.0f, bot = 0.0f;
    int idx = player->vine_index;

    if (player->climb_source == 0) {
        grab = player_vine_grab_rect(&vines[idx]);
        int vh = (vines[idx].tile_count - 1) * VINE_STEP + VINE_H;
        top = vines[idx].y;
        bot = vines[idx].y + (float)vh;
    } else if (player->climb_source == 1) {
        grab = player_ladder_grab_rect(&ladders[idx]);
        int lh = (ladders[idx].tile_count - 1) * LADDER_STEP + LADDER_H;
        top = ladders[idx].y;
        bot = ladders[idx].y + (float)lh;
    } else {
        grab = player_rope_grab_rect(&ropes[idx]);
        int rh = (ropes[idx].tile_count - 1) * ROPE_STEP + ROPE_H;
        top = ropes[idx].y;
        bot = ropes[idx].y + (float)rh;
    }

    *out_grab   = grab;
    *out_top    = top;
    *out_bottom = bot;
}
