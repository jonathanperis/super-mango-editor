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
#include "../game_constants.h"      /* FLOOR_Y, GAME_W, FLOOR_GAP_W */
#include "../core/entity_utils.h"  /* patrol_update, animate_frame_ms */

#define JSPIDER_AUDIBLE_RANGE  ((float)GAME_W)
#define JSPIDER_VOL_MAX        128

/* ------------------------------------------------------------------ */

void jumping_spiders_update(JumpingSpider *spiders, int count, float dt,
                            const int *floor_gaps, int floor_gap_count,
                            SoundEffect *snd_attack, float player_x, int cam_x)
{
    for (int i = 0; i < count; i++) {
        JumpingSpider *s = &spiders[i];

        /* ── horizontal movement + patrol boundary reversal ──────── */
        patrol_update(&s->x, &s->vx, JSPIDER_FRAME_W,
                      s->patrol_x0, s->patrol_x1, dt);

        /* ── floor gap interaction ────────────────────────────────── */
        /*
         * Check if the spider's art centre is over a gap.
         *   - On the ground → trigger a jump to clear the gap.
         *   - Airborne       → do nothing; let the spider fly over.
         */
        float art_center = s->x + JSPIDER_ART_X + JSPIDER_ART_W / 2.0f;
        if (s->on_ground) {
            for (int g = 0; g < floor_gap_count; g++) {
                float gx = (float)floor_gaps[g];
                if (art_center >= gx && art_center < gx + (float)FLOOR_GAP_W) {
                    /* Snap back to the gap edge and leap */
                    if (s->vx > 0.0f)
                        s->x = gx - JSPIDER_ART_X - JSPIDER_ART_W / 2.0f;
                    else
                        s->x = gx + (float)FLOOR_GAP_W - JSPIDER_ART_X - JSPIDER_ART_W / 2.0f;

                    s->vy        = JSPIDER_JUMP_VY;
                    s->on_ground = 0;

                    /* Play attack sound with distance-based volume */
                    if (snd_attack) {
                        float spider_cx = s->x + JSPIDER_ART_X + JSPIDER_ART_W / 2.0f;
                        int on_screen = (spider_cx >= (float)cam_x - JSPIDER_FRAME_W &&
                                         spider_cx <= (float)cam_x + GAME_W + JSPIDER_FRAME_W);
                        if (on_screen) {
                            float dist = fabsf(player_x - spider_cx);
                            int vol = sound_volume_for_distance(dist, JSPIDER_AUDIBLE_RANGE, JSPIDER_VOL_MAX);
                            if (vol > 0) {
                                sound_play(snd_attack, vol);
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
        animate_frame_ms(&s->frame_index, &s->anim_timer_ms,
                         dt, JSPIDER_FRAME_MS, JSPIDER_FRAMES);
    }
}

/* ------------------------------------------------------------------ */

void jumping_spiders_render(const JumpingSpider *spiders, int count,
                            Texture2D *tex,
                            int cam_x)
{
    for (int i = 0; i < count; i++) {
        const JumpingSpider *s = &spiders[i];

        /*
         * Source rect: the 10-px tall art band of the current frame.
         * Same layout as Spider_1.png — crop to visible art rows.
         */
        IntRect src = {
            s->frame_index * JSPIDER_FRAME_W,
            JSPIDER_ART_Y,
            JSPIDER_FRAME_W,
            JSPIDER_ART_H
        };

        /*
         * Destination rect: y uses FLOOR_Y minus art height, offset by
         * the vertical jump displacement (s->y is negative when airborne).
         */
        IntRect dst = {
            (int)s->x - cam_x,
            FLOOR_Y - JSPIDER_ART_H + (int)s->y,
            JSPIDER_FRAME_W,
            JSPIDER_ART_H
        };

        /*
         * The base sprite faces left; flip horizontally when walking right.
         */
        int flip = s->vx > 0.0f ? SPRITE_FLIP_X : SPRITE_NORMAL;
        sprite_draw(tex, &src, &dst, 0, flip, WHITE);
    }
}
