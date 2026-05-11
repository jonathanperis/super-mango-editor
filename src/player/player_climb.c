/*
 * player_climb.c — Grab-zone helpers for vine/ladder/rope climbing.
 */

#include "player_climb.h"

#include "player_animation.h"
#include "player_internal.h"

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
 * player_ladder_grab_rect — Return the axis-aligned grab zone for a ladder.
 *
 * The ladder sprite uses its own width and vertical step constants, but the
 * horizontal forgiveness matches vines so grabbing either climbable feels the
 * same to the player.
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
 * player_rope_grab_rect — Return the axis-aligned grab zone for a rope.
 *
 * Ropes share the same forgiving horizontal pad as vines and ladders while
 * using rope-specific tile spacing to cover the full climbable height.
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

int player_try_grab_climbable(Player *player,
                              const VineDecor *vines, int vine_count,
                              const LadderDecor *ladders, int ladder_count,
                              const RopeDecor *ropes, int rope_count) {
    SDL_Rect phit = player_get_hitbox(player);
    int grabbed = 0;

    /* Check vines first so existing level ordering remains unchanged. */
    for (int i = 0; i < vine_count && !grabbed; i++) {
        SDL_Rect vgrab = player_vine_grab_rect(&vines[i]);
        if (SDL_HasIntersection(&phit, &vgrab)) {
            player->on_vine      = 1;
            player->vine_index   = i;
            player->climb_source = 0;
            grabbed = 1;
        }
    }

    for (int i = 0; i < ladder_count && !grabbed; i++) {
        SDL_Rect lgrab = player_ladder_grab_rect(&ladders[i]);
        if (SDL_HasIntersection(&phit, &lgrab)) {
            player->on_vine      = 1;
            player->vine_index   = i;
            player->climb_source = 1;
            grabbed = 1;
        }
    }

    for (int i = 0; i < rope_count && !grabbed; i++) {
        SDL_Rect rgrab = player_rope_grab_rect(&ropes[i]);
        if (SDL_HasIntersection(&phit, &rgrab)) {
            player->on_vine      = 1;
            player->vine_index   = i;
            player->climb_source = 2;
            grabbed = 1;
        }
    }

    if (grabbed) {
        player->on_ground = 0;
        player->vy        = 0.0f;
        player->vx        = 0.0f;
    }

    return grabbed;
}

/*
 * player_climb_get_bounds — Return the grab rect, top y, and bottom y of the
 * currently climbed object, regardless of its type (vine/ladder/rope).
 *
 * player->climb_source selects which climbable array to read, and
 * player->vine_index selects the active element inside that array.  The update
 * path uses the outputs to clamp the player at the top and detach at the
 * bottom or when drifting outside the grab rectangle.
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

int player_update_climbing(Player *player, float dt,
                           const VineDecor *vines,
                           const LadderDecor *ladders,
                           const RopeDecor *ropes,
                           int world_w) {
    if (!player->on_vine) {
        return 0;
    }

    player->coyote_timer = 0.0f;
    player->jump_buffer_timer = 0.0f;
    player->x += player->vx * dt;
    player->y += player->vy * dt;

    SDL_Rect grab;
    float climb_top, climb_bottom;
    player_climb_get_bounds(player, vines, ladders, ropes,
                            &grab, &climb_top, &climb_bottom);
    SDL_Rect phit = player_get_hitbox(player);

    /* Horizontal detach — vertical bounds are handled below. */
    const int x_overlap =
        (phit.x < grab.x + grab.w) && (phit.x + phit.w > grab.x);
    if (!x_overlap) {
        player->on_vine = 0;
        player->vy      = 0.0f;
        player_animate(player, (Uint32)(dt * 1000.0f));
        return 1;
    }

    /* Vertical clamp at climbable top. */
    if (player->y + PHYS_PAD_TOP < climb_top) {
        player->y  = climb_top - (float)PHYS_PAD_TOP;
        player->vy = 0.0f;
    }

    /* Bottom detach — feet below climbable bottom → release and fall. */
    float player_bottom = player->y + player->h - FLOOR_SINK;
    if (player_bottom > climb_bottom) {
        player->on_vine = 0;
        player->vy      = 0.0f;
    }

    /* Horizontal world clamp (same logic as normal path). */
    if (player->x + PHYS_PAD_X < 0.0f)
        player->x = -(float)PHYS_PAD_X;
    if (player->x + player->w - PHYS_PAD_X > world_w)
        player->x = (float)(world_w - player->w + PHYS_PAD_X);

    player_animate(player, (Uint32)(dt * 1000.0f));
    return 1;
}
