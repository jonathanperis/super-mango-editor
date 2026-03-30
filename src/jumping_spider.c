/*
 * jumping_spider.c — Jumping spider enemy: ground patrol + gap jumping.
 *
 * Behaves like a regular spider (horizontal patrol, animation) but instead
 * of reversing at sea gaps, it jumps over them.  The spider patrols across
 * a range that spans one or more gaps.  When its art centre reaches a gap
 * edge while on the ground, it leaps and clears the hole in mid-air,
 * landing on solid ground on the other side and continuing its patrol.
 *
 * Jump physics: at 55 px/s horizontal speed, vy = −200, gravity = 600,
 * the spider is airborne for ~0.67 s and covers ~37 px — enough to clear
 * a 32-px sea gap.
 */
#include <math.h>   /* fabsf */

#include "jumping_spider.h"
#include "game.h"   /* FLOOR_Y, GAME_W, SEA_GAP_W */

#define JSPIDER_AUDIBLE_RANGE  ((float)GAME_W)
#define JSPIDER_VOL_MAX        128
#define JSPIDER_VOL_MIN        16

static int jspider_volume_for_distance(float dist) {
    if (dist >= JSPIDER_AUDIBLE_RANGE) return 0;
    if (dist <= 0.0f) return JSPIDER_VOL_MAX;
    float fraction = dist / JSPIDER_AUDIBLE_RANGE;
    return JSPIDER_VOL_MAX - (int)(fraction * (JSPIDER_VOL_MAX - JSPIDER_VOL_MIN));
}

/* ------------------------------------------------------------------ */

void jumping_spiders_init(JumpingSpider *spiders, int *count)
{
    *count = 1;

    /*
     * Jumping Spider 0 — patrols screen 1 across the sea gap at x=192–224.
     *
     * Patrol range 46–310 spans from the first ground coin to just before
     * the bouncepad at x=310.  The spider walks right from x=130, reaches
     * the gap at 192, jumps over, continues to 310, reverses, walks back,
     * jumps over the gap again at 224, and repeats.
     */
    spiders[0].x             = 130.0f;
    spiders[0].y             = 0.0f;      /* on the ground */
    spiders[0].vx            = JSPIDER_SPEED;
    spiders[0].vy            = 0.0f;
    spiders[0].patrol_x0     = 46.0f;     /* after first coin */
    spiders[0].patrol_x1     = 310.0f;    /* before bouncepad at 310 */
    spiders[0].jump_timer    = 0.0f;      /* unused — jumps are gap-triggered */
    spiders[0].on_ground     = 1;
    spiders[0].frame_index   = 0;
    spiders[0].anim_timer_ms = 0;
}

/* ------------------------------------------------------------------ */

void jumping_spiders_update(JumpingSpider *spiders, int count, float dt,
                            const int *sea_gaps, int sea_gap_count,
                            Mix_Chunk *snd_attack, float player_x, int cam_x)
{
    for (int i = 0; i < count; i++) {
        JumpingSpider *s = &spiders[i];

        /* ── horizontal movement ──────────────────────────────────── */
        s->x += s->vx * dt;

        /* Patrol boundary reversal */
        if (s->vx > 0.0f && s->x + JSPIDER_FRAME_W >= s->patrol_x1) {
            s->x  = s->patrol_x1 - JSPIDER_FRAME_W;
            s->vx = -JSPIDER_SPEED;
        } else if (s->vx < 0.0f && s->x <= s->patrol_x0) {
            s->x  = s->patrol_x0;
            s->vx =  JSPIDER_SPEED;
        }

        /* ── sea gap interaction ──────────────────────────────────── */
        /*
         * Check if the spider's art centre is over a gap.
         *   - On the ground → trigger a jump to clear the gap.
         *   - Airborne       → do nothing; let the spider fly over.
         */
        float art_center = s->x + JSPIDER_ART_X + JSPIDER_ART_W / 2.0f;
        if (s->on_ground) {
            for (int g = 0; g < sea_gap_count; g++) {
                float gx = (float)sea_gaps[g];
                if (art_center >= gx && art_center < gx + (float)SEA_GAP_W) {
                    /* Snap back to the gap edge and leap */
                    if (s->vx > 0.0f)
                        s->x = gx - JSPIDER_ART_X - JSPIDER_ART_W / 2.0f;
                    else
                        s->x = gx + (float)SEA_GAP_W - JSPIDER_ART_X - JSPIDER_ART_W / 2.0f;

                    s->vy        = JSPIDER_JUMP_VY;
                    s->on_ground = 0;

                    /* Play attack sound with distance-based volume */
                    if (snd_attack) {
                        float spider_cx = s->x + JSPIDER_ART_X + JSPIDER_ART_W / 2.0f;
                        int on_screen = (spider_cx >= (float)cam_x - JSPIDER_FRAME_W &&
                                         spider_cx <= (float)cam_x + GAME_W + JSPIDER_FRAME_W);
                        if (on_screen) {
                            float dist = fabsf(player_x - spider_cx);
                            int vol = jspider_volume_for_distance(dist);
                            if (vol > 0) {
                                int ch = Mix_PlayChannel(-1, snd_attack, 0);
                                if (ch >= 0) Mix_Volume(ch, vol);
                            }
                        }
                    }
                    break;
                }
            }
        }
        /* (airborne spiders skip gap checks — they fly right over) */

        /* ── vertical movement ────────────────────────────────────── */
        if (!s->on_ground) {
            s->vy += JSPIDER_GRAVITY * dt;
            s->y  += s->vy * dt;

            if (s->y >= 0.0f) {
                /* Landed — snap back to ground level */
                s->y         = 0.0f;
                s->vy        = 0.0f;
                s->on_ground = 1;
            }
        }

        /* ── advance animation frame ──────────────────────────────── */
        s->anim_timer_ms += (Uint32)(dt * 1000.0f);
        if (s->anim_timer_ms >= JSPIDER_FRAME_MS) {
            s->anim_timer_ms -= JSPIDER_FRAME_MS;
            s->frame_index    = (s->frame_index + 1) % JSPIDER_FRAMES;
        }
    }
}

/* ------------------------------------------------------------------ */

void jumping_spiders_render(const JumpingSpider *spiders, int count,
                            SDL_Renderer *renderer, SDL_Texture *tex,
                            int cam_x)
{
    for (int i = 0; i < count; i++) {
        const JumpingSpider *s = &spiders[i];

        /*
         * Source rect: the 10-px tall art band of the current frame.
         * Same layout as Spider_1.png — crop to visible art rows.
         */
        SDL_Rect src = {
            s->frame_index * JSPIDER_FRAME_W,
            JSPIDER_ART_Y,
            JSPIDER_FRAME_W,
            JSPIDER_ART_H
        };

        /*
         * Destination rect: y uses FLOOR_Y minus art height, offset by
         * the vertical jump displacement (s->y is negative when airborne).
         */
        SDL_Rect dst = {
            (int)s->x - cam_x,
            FLOOR_Y - JSPIDER_ART_H + (int)s->y,
            JSPIDER_FRAME_W,
            JSPIDER_ART_H
        };

        /*
         * The base sprite faces left; flip horizontally when walking right.
         */
        SDL_RendererFlip flip = (s->vx > 0.0f)
                                 ? SDL_FLIP_HORIZONTAL
                                 : SDL_FLIP_NONE;
        SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL, flip);
    }
}
