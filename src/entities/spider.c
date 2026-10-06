/*
 * spider.c — Spider enemy: ground patrol, 4-frame animation.
 *
 * Each spider walks back and forth inside a fixed patrol range on the
 * ground floor.  It reverses direction at the patrol boundaries, and its
 * sprite is flipped horizontally to face the direction of travel.
 * No player collision is handled here; that comes in a later pass.
 */
#include "spider.h"
#include "../game.h"                /* FLOOR_Y, GAME_W, FLOOR_GAP_W */
#include "../core/entity_utils.h"  /* patrol_update, patrol_gap_reverse, animate_frame_ms */

/* ------------------------------------------------------------------ */

void spiders_update(Spider *spiders, int count, float dt,
                    const int *floor_gaps, int floor_gap_count)
{
    for (int i = 0; i < count; i++) {
        Spider *s = &spiders[i];

        /* ── move horizontally + patrol boundary reversal ─────────── */
        patrol_update(&s->x, &s->vx, SPIDER_FRAME_W,
                      s->patrol_x0, s->patrol_x1, SPIDER_SPEED, dt);

        /*
         * Floor gap check — reverse if the spider's art centre would be
         * over a hole in the ground, preventing them from floating in mid-air.
         */
        patrol_gap_reverse(&s->x, &s->vx,
                           SPIDER_ART_X, SPIDER_ART_W, SPIDER_SPEED,
                           floor_gaps, floor_gap_count, FLOOR_GAP_W);

        /* ── advance animation frame ───────────────────────────────── */
        animate_frame_ms(&s->frame_index, &s->anim_timer_ms,
                         dt, SPIDER_FRAME_MS, SPIDER_FRAMES);
    }
}

/* ------------------------------------------------------------------ */

void spiders_render(const Spider *spiders, int count,
                    Texture2D *tex, int cam_x)
{
    for (int i = 0; i < count; i++) {
        const Spider *s = &spiders[i];

        /*
         * Source rect: the 10-px tall art band of the current frame.
         *   x = frame_index × SPIDER_FRAME_W  — selects the correct column.
         *   y = SPIDER_ART_Y (22)              — skips the transparent top.
         *   w = SPIDER_FRAME_W (48)            — full frame width.
         *   h = SPIDER_ART_H  (10)             — only the visible art rows.
         */
        IntRect src = {
            s->frame_index * SPIDER_FRAME_W,
            SPIDER_ART_Y,
            SPIDER_FRAME_W,
            SPIDER_ART_H
        };

        /*
         * Destination rect: world → screen by subtracting cam_x from x.
         * y keeps the art bottom flush with FLOOR_Y (252 - 10 = 242).
         */
        IntRect dst = {
            (int)s->x - cam_x,   /* world-space x converted to screen-space */
            FLOOR_Y - SPIDER_ART_H,
            SPIDER_FRAME_W,
            SPIDER_ART_H
        };

        /*
         * The base sprite faces left, so flip horizontally when the spider
         * walks right (vx > 0).
         */
        int flip = s->vx > 0.0f ? SPRITE_FLIP_X : SPRITE_NORMAL;
        sprite_draw(tex, &src, &dst, 0, flip, WHITE);
    }
}
