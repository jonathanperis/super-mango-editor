/*
 * bridge.c — Tiled bridge with brick-by-brick crumble-fall.
 *
 * Each brick is an independent 16×16 tile that falls on its own schedule.
 * When the player first touches the bridge, the brick nearest to the player
 * begins falling after BRIDGE_STAND_LIMIT seconds.  Adjacent bricks follow
 * with a staggered CASCADE_DELAY, creating a domino ripple outward from
 * the contact point.
 */
#include <SDL.h>
#include <math.h>   /* fabsf */

#include "bridge.h"
#include "game.h"   /* FLOOR_Y, TILE_SIZE, GAME_H */

/* ------------------------------------------------------------------ */

void bridges_init(Bridge *bridges, int *count)
{
    *count = 1;

    /*
     * Bridge 0 — spans between the last two pillars (screen 4).
     *
     * Pillar 7: x=1300, w=48 → right edge 1348.
     * Pillar 8: x=1480 → left edge 1480.
     * 8 bricks × 16 px = 128 px, centred in the 132-px gap → x=1350.
     * y matches the pillar top: FLOOR_Y − 2×TILE_SIZE + 16 = 172.
     */
    Bridge *b   = &bridges[0];
    b->x          = 1350.0f;
    b->base_y     = (float)(FLOOR_Y - 2 * TILE_SIZE + 16);
    b->brick_count = 8;

    for (int i = 0; i < b->brick_count; i++) {
        b->bricks[i].y_offset   = 0.0f;
        b->bricks[i].fall_vy    = 0.0f;
        b->bricks[i].falling    = 0;
        b->bricks[i].active     = 1;
        b->bricks[i].fall_delay = -1.0f;  /* untouched; set to 0 on first contact */
    }
}

/* ------------------------------------------------------------------ */

void bridges_update(Bridge *bridges, int count, float dt,
                    int landed_idx, float player_cx)
{
    for (int bi = 0; bi < count; bi++) {
        Bridge *b = &bridges[bi];

        /*
         * If the player is standing on this bridge, find the brick
         * under their centre and start its individual fall timer.
         * Each brick tracks its own timer independently — only the
         * bricks the player actually steps on will fall.
         */
        if (landed_idx == bi) {
            int idx = (int)((player_cx - b->x) / BRIDGE_TILE_W);
            if (idx >= 0 && idx < b->brick_count) {
                BridgeBrick *touched = &b->bricks[idx];
                if (touched->active && !touched->falling &&
                    touched->fall_delay < 0.0f) {
                    /*
                     * First contact — mark the brick as triggered
                     * immediately.  Set fall_delay to 0 to start the
                     * countdown toward BRIDGE_FALL_DELAY.
                     */
                    touched->fall_delay = 0.0f;
                }
            }
        }

        /* ── advance each brick's fall state ──────────────────────── */
        for (int i = 0; i < b->brick_count; i++) {
            BridgeBrick *br = &b->bricks[i];
            if (!br->active) continue;

            if (!br->falling) {
                /* Only count down bricks that have been touched (delay >= 0) */
                if (br->fall_delay >= 0.0f) {
                    br->fall_delay += dt;
                    if (br->fall_delay >= BRIDGE_FALL_DELAY) {
                        br->falling = 1;
                        br->fall_vy = BRIDGE_FALL_INITIAL_VY;
                    }
                }
            } else {
                /* Smooth fall: gentle gravity per brick */
                br->fall_vy  += BRIDGE_FALL_GRAVITY * dt;
                br->y_offset += br->fall_vy * dt;

                if (b->base_y + br->y_offset > (float)(GAME_H + 64)) {
                    br->active = 0;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */

SDL_Rect bridge_get_rect(const Bridge *b)
{
    SDL_Rect r;
    r.x = (int)b->x;
    r.y = (int)b->base_y;
    r.w = b->brick_count * BRIDGE_TILE_W;
    r.h = BRIDGE_TILE_H;
    return r;
}

/* ------------------------------------------------------------------ */

int bridge_has_solid_at(const Bridge *b, float wx)
{
    if (wx < b->x || wx >= b->x + b->brick_count * BRIDGE_TILE_W)
        return 0;
    int idx = (int)((wx - b->x) / BRIDGE_TILE_W);
    if (idx < 0 || idx >= b->brick_count) return 0;
    return b->bricks[idx].active && !b->bricks[idx].falling;
}

/* ------------------------------------------------------------------ */

void bridges_render(const Bridge *bridges, int count,
                    SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    if (!tex) return;

    for (int bi = 0; bi < count; bi++) {
        const Bridge *b = &bridges[bi];

        for (int i = 0; i < b->brick_count; i++) {
            const BridgeBrick *br = &b->bricks[i];
            if (!br->active) continue;

            SDL_Rect src = { 0, 0, BRIDGE_TILE_W, BRIDGE_TILE_H };
            SDL_Rect dst = {
                (int)b->x + i * BRIDGE_TILE_W - cam_x,
                (int)(b->base_y + br->y_offset),
                BRIDGE_TILE_W,
                BRIDGE_TILE_H
            };
            SDL_RenderCopy(renderer, tex, &src, &dst);
        }
    }
}
