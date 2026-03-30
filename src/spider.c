/*
 * spider.c — Spider enemy: ground patrol, 4-frame animation.
 *
 * Each spider walks back and forth inside a fixed patrol range on the
 * ground floor.  It reverses direction at the patrol boundaries, and its
 * sprite is flipped horizontally to face the direction of travel.
 * No player collision is handled here; that comes in a later pass.
 */
#include "spider.h"
#include "game.h"   /* FLOOR_Y, GAME_W */

/* ------------------------------------------------------------------ */

void spiders_init(Spider *spiders, int *count)
{
    *count = 2;

    /*
     * Spider 0 — patrols screen 2–3, east of the sea gap at x=560–592.
     * Patrol starts after the gap so the debug range matches movement.
     */
    spiders[0].x             = 600.0f;
    spiders[0].vx            = SPIDER_SPEED;
    spiders[0].patrol_x0     = 592.0f;
    spiders[0].patrol_x1     = 750.0f;
    spiders[0].frame_index   = 0;
    spiders[0].anim_timer_ms = 0;

    /*
     * Spider 1 — patrols screen 3–4, west of the sea gap at x=1152–1184.
     * Patrol ends before the gap so the debug range matches movement.
     */
    spiders[1].x             = 1100.0f;
    spiders[1].vx            = -SPIDER_SPEED;
    spiders[1].patrol_x0     = 1000.0f;
    spiders[1].patrol_x1     = 1152.0f;
    spiders[1].frame_index   = 1;
    spiders[1].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void spiders_update(Spider *spiders, int count, float dt,
                    const int *sea_gaps, int sea_gap_count)
{
    for (int i = 0; i < count; i++) {
        Spider *s = &spiders[i];

        /* ── move horizontally ─────────────────────────────────────── */
        s->x += s->vx * dt;

        /*
         * Patrol boundary check: when the right edge of the frame
         * slot reaches patrol_x1, or the left edge drops below patrol_x0,
         * snap position to the boundary and reverse velocity.
         */
        if (s->vx > 0.0f && s->x + SPIDER_FRAME_W >= s->patrol_x1) {
            s->x  = s->patrol_x1 - SPIDER_FRAME_W;
            s->vx = -SPIDER_SPEED;
        } else if (s->vx < 0.0f && s->x <= s->patrol_x0) {
            s->x  = s->patrol_x0;
            s->vx =  SPIDER_SPEED;
        }

        /*
         * Sea gap check — reverse if the spider's art centre would be
         * over a hole in the ground.  This prevents them from walking
         * across gaps and floating in mid-air.
         */
        float art_center = s->x + SPIDER_ART_X + SPIDER_ART_W / 2.0f;
        for (int g = 0; g < sea_gap_count; g++) {
            float gx = (float)sea_gaps[g];
            if (art_center >= gx && art_center < gx + (float)SEA_GAP_W) {
                /* Snap back to the gap edge and reverse direction */
                if (s->vx > 0.0f) {
                    s->x  = gx - SPIDER_ART_X - SPIDER_ART_W;
                    s->vx = -SPIDER_SPEED;
                } else {
                    s->x  = gx + (float)SEA_GAP_W - SPIDER_ART_X;
                    s->vx =  SPIDER_SPEED;
                }
                break;
            }
        }

        /* ── advance animation frame ───────────────────────────────── */
        s->anim_timer_ms += (Uint32)(dt * 1000.0f);
        if (s->anim_timer_ms >= SPIDER_FRAME_MS) {
            s->anim_timer_ms -= SPIDER_FRAME_MS;
            s->frame_index    = (s->frame_index + 1) % SPIDER_FRAMES;
        }
    }
}

/* ------------------------------------------------------------------ */

void spiders_render(const Spider *spiders, int count,
                    SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
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
        SDL_Rect src = {
            s->frame_index * SPIDER_FRAME_W,
            SPIDER_ART_Y,
            SPIDER_FRAME_W,
            SPIDER_ART_H
        };

        /*
         * Destination rect: world → screen by subtracting cam_x from x.
         * y keeps the art bottom flush with FLOOR_Y (252 - 10 = 242).
         */
        SDL_Rect dst = {
            (int)s->x - cam_x,   /* world-space x converted to screen-space */
            FLOOR_Y - SPIDER_ART_H,
            SPIDER_FRAME_W,
            SPIDER_ART_H
        };

        /*
         * The base sprite faces left, so flip horizontally when the spider
         * walks right (vx > 0).
         */
        SDL_RendererFlip flip = (s->vx > 0.0f)
                                ? SDL_FLIP_HORIZONTAL
                                : SDL_FLIP_NONE;
        SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL, flip);
    }
}
