/*
 * bouncepad.c — Implementation of Bouncepad init, update, and render.
 *
 * The bouncepad sits on the floor and acts as a one-way launch surface.
 * Collision detection lives in player_update (player.c), which applies
 * BOUNCEPAD_VY on landing and returns the hit index to game_loop.
 * game_loop then sets state = BOUNCE_ACTIVE here to start the animation.
 */

#include <SDL.h>
#include <stdio.h>

#include "bouncepad.h"
#include "../game.h"   /* FLOOR_Y, TILE_SIZE */

/* ------------------------------------------------------------------ */

/*
 * bouncepad_place — Initialise one pad instance with position and type.
 *
 * All three variant inits (small/medium/high) call this instead of setting
 * every field manually.  Only x, launch_vy and pad_type differ between
 * variants; everything else is always the same default state.
 */
void bouncepad_place(Bouncepad *pad, float x, float launch_vy, BouncepadType pad_type)
{
    pad->x             = x;
    pad->y             = (float)(FLOOR_Y - BOUNCEPAD_SRC_H);
    pad->w             = BOUNCEPAD_W;
    pad->h             = BOUNCEPAD_SRC_H;
    pad->state         = BOUNCE_IDLE;
    pad->anim_frame    = 2;   /* frame 2 = compressed idle pose */
    pad->anim_timer_ms = 0;
    pad->launch_vy     = launch_vy;
    pad->pad_type      = pad_type;
}

/* ------------------------------------------------------------------ */

/*
 * bouncepads_update — Advance the squash/release animation each frame.
 *
 * Only ACTIVE pads move through the sequence.  The sequence is:
 *   frame 1 (80 ms) → frame 0 (80 ms) → IDLE (frame 2)
 *
 * dt_ms is the frame delta time in milliseconds, consistent with how
 * the rest of the animation timers in this codebase work.
 */
void bouncepads_update(Bouncepad *pads, int count, Uint32 dt_ms) {
    for (int i = 0; i < count; i++) {
        Bouncepad *p = &pads[i];
        if (p->state != BOUNCE_ACTIVE) continue;

        p->anim_timer_ms += dt_ms;

        if (p->anim_timer_ms >= BOUNCEPAD_FRAME_MS) {
            p->anim_timer_ms -= BOUNCEPAD_FRAME_MS;  /* carry over remainder */
            p->anim_frame--;                          /* 1 → 0               */

            if (p->anim_frame < 0) {
                /*
                 * Animation finished: return to idle compressed pose.
                 * anim_frame 2 is the compressed resting state (third image).
                 */
                p->anim_frame = 2;
                p->state      = BOUNCE_IDLE;
            }
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * bouncepads_render — Blit each pad's current frame to the back buffer.
 *
 * The sprite sheet is a single row of three 48-px-wide frames.  Each
 * frame is cropped vertically to rows BOUNCEPAD_SRC_Y..+BOUNCEPAD_SRC_H
 * (the art-only region) so transparent padding is never rendered.
 *
 * World → screen conversion: dst.x = (int)pad->x - cam_x.
 */
void bouncepads_render(const Bouncepad *pads, int count,
                       SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!tex) return;   /* texture failed to load; skip silently */

    for (int i = 0; i < count; i++) {
        const Bouncepad *p = &pads[i];

        /*
         * src — cut the art-only region from the correct frame column.
         *
         * Each frame is 48 px wide; the visible art spans rows 14–31
         * (BOUNCEPAD_SRC_Y=14, BOUNCEPAD_SRC_H=18).  Cropping avoids
         * blitting the ~14 rows of transparent padding above and the
         * ~16 rows below, keeping the rendered rect tight to the art.
         *
         * anim_frame 0 → src.x=0, frame 1 → src.x=48, frame 2 → src.x=96.
         */
        SDL_Rect src = {
            p->anim_frame * BOUNCEPAD_W,  /* column within the sheet */
            BOUNCEPAD_SRC_Y,              /* skip transparent top padding */
            BOUNCEPAD_W,
            BOUNCEPAD_SRC_H               /* only the art rows            */
        };

        /*
         * dst — world to screen: subtract cam_x so the pad scrolls with
         * the camera exactly like every other world-space object.
         * Height matches the cropped src so the scale stays 1:1.
         */
        SDL_Rect dst = {
            (int)p->x - cam_x,
            (int)p->y,
            p->w,
            p->h
        };

        SDL_RenderCopy(renderer, tex, &src, &dst);
    }
}
