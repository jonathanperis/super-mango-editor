/*
 * axe_trap.c — Implementation of AxeTrap init, update, render.
 *
 * Each axe trap hangs from the top-centre of a tall platform pillar and
 * swings like a grandfather clock pendulum (sinusoidal oscillation) or
 * spins continuously through 360°.  A sound effect plays at the swing
 * extremes / rotation completion.
 */

#include <SDL.h>
#include <math.h>    /* sinf, fabsf, fmodf, M_PI */
#include <stdio.h>

#include "axe_trap.h"
#include "game.h"    /* FLOOR_Y, TILE_SIZE, platforms — for placement */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */

/*
 * axe_traps_init — Place axe traps on the tall (3-tile) pillars.
 *
 * Tall pillars (platforms with h == 3 * TILE_SIZE = 144 px) exist at
 * indices 1, 3, 5 in the platforms array (see platform.c).
 *
 * We place two axe traps:
 *   Trap 0 — on platform 1 (x=256), pendulum mode (grandfather clock swing).
 *   Trap 1 — on platform 3 (x=680), full 360° rotation.
 *
 * The pivot point is positioned at the horizontal centre of the pillar,
 * vertically at the top of the pillar (platform y).
 */
void axe_traps_init(AxeTrap *traps, int *count) {
    /*
     * Trap 0 — pendulum axe on the tall pillar at x=256 (screen 1).
     *
     * Pivot x: pillar left edge (256) + half pillar width (48/2 = 24) = 280.
     * Pivot y: pillar top (124) — the axe hangs from the top surface.
     */
    traps[0].x            = 256.0f + TILE_SIZE / 2.0f;  /* 280 */
    traps[0].y            = (float)(FLOOR_Y - 3 * TILE_SIZE + 16);  /* 124 */
    traps[0].angle        = 0.0f;
    traps[0].time         = 0.0f;
    traps[0].mode         = AXE_MODE_PENDULUM;
    traps[0].sound_played = 0;
    traps[0].active       = 1;

    /*
     * Trap 1 — spinning axe on the tall pillar at x=680 (screen 2).
     *
     * Pivot x: 680 + 24 = 704.
     * Pivot y: same height as other tall pillars = 124.
     */
    traps[1].x            = 680.0f + TILE_SIZE / 2.0f;  /* 704 */
    traps[1].y            = (float)(FLOOR_Y - 3 * TILE_SIZE + 16);  /* 124 */
    traps[1].angle        = 0.0f;
    traps[1].time         = 0.0f;
    traps[1].mode         = AXE_MODE_SPIN;
    traps[1].sound_played = 0;
    traps[1].active       = 1;

    *count = 2;
}

/* ------------------------------------------------------------------ */

/*
 * AXE_VOLUME_MAX — the loudest the axe SFX can be (MIX_MAX_VOLUME = 128).
 * AXE_AUDIBLE_RANGE — maximum horizontal distance (px) at which the axe
 *                     is audible.  Set to GAME_W (the logical canvas width)
 *                     so the sound fades to complete silence exactly when the
 *                     player is one full screen-width away from the axe.
 */
#define AXE_VOLUME_MAX       128
#define AXE_AUDIBLE_RANGE    ((float)GAME_W)

/*
 * axe_volume_for_distance — Compute the Mix_Chunk volume (0–128)
 * based on horizontal distance between the player and the axe pivot.
 *
 * At distance 0            → AXE_VOLUME_MAX (128, full volume).
 * At distance AUDIBLE_RANGE → 0 (completely silent).
 * Beyond AUDIBLE_RANGE     → 0 (silent).
 *
 * Linear interpolation gives a smooth, gradual fade from full volume
 * to silence as the player moves one screen-width away from the source.
 */
static int axe_volume_for_distance(float dist) {
    if (dist >= AXE_AUDIBLE_RANGE) return 0;
    if (dist <= 0.0f) return AXE_VOLUME_MAX;

    /*
     * Linear ramp from MAX at dist=0 to 0 at dist=AUDIBLE_RANGE.
     * (1 - fraction) goes from 1.0 (closest) to 0.0 (farthest audible).
     */
    float fraction = dist / AXE_AUDIBLE_RANGE;
    return (int)((1.0f - fraction) * AXE_VOLUME_MAX);
}

/* ------------------------------------------------------------------ */

/*
 * axe_traps_update — Advance swing/spin animation and trigger sound.
 *
 * The animation always advances (even off-screen) so the axe isn't frozen
 * mid-swing when the player scrolls back.  Sound, however, only plays
 * when the axe is on-screen, and its volume scales with player proximity:
 * louder when close, quieter at screen edge, silent off-screen.
 *
 * Pendulum mode:
 *   angle = AMPLITUDE × sin(2π × time / PERIOD)
 *   Sound fires when |angle| exceeds 95% of amplitude (near extremes).
 *   Resets when the axe passes back through centre (|angle| < 10°).
 *
 * Spin mode:
 *   angle += SPIN_SPEED × dt (continuous clockwise rotation).
 *   Sound fires once per full 360° rotation.
 */
void axe_traps_update(AxeTrap *traps, int count, float dt,
                      Mix_Chunk *snd_axe, float player_x, int cam_x) {
    for (int i = 0; i < count; i++) {
        AxeTrap *t = &traps[i];
        if (!t->active) continue;

        /*
         * Compute horizontal distance between the player and the axe pivot.
         * Used for volume scaling — closer player = louder whoosh.
         */
        float dist = fabsf(player_x - t->x);
        int vol = axe_volume_for_distance(dist);

        /*
         * On-screen check — the axe is visible when its pivot falls
         * within [cam_x - margin, cam_x + GAME_W + margin].
         * The margin accounts for the sprite width so the axe doesn't
         * pop in/out at the screen edges.
         */
        int on_screen = (t->x >= (float)cam_x - AXE_DISPLAY_W &&
                         t->x <= (float)cam_x + GAME_W + AXE_DISPLAY_W);

        if (t->mode == AXE_MODE_PENDULUM) {
            /*
             * Pendulum oscillation — driven by accumulated time.
             *
             * The angular frequency ω = 2π / PERIOD converts seconds to
             * radians.  sin(ω × time) produces a smooth −1..+1 wave.
             * Multiplying by AMPLITUDE scales that to the desired swing arc.
             */
            t->time += dt;
            float omega = 2.0f * (float)M_PI / AXE_SWING_PERIOD;
            t->angle = AXE_SWING_AMPLITUDE * sinf(omega * t->time);

            /*
             * Sound trigger — fire once when the axe nears a swing extreme.
             *
             * 95% of amplitude = the "whoosh" moment where the blade is at
             * its fastest angular velocity (just before direction reversal).
             * sound_played prevents re-triggering on the same half-cycle;
             * it resets when the axe crosses back through centre (|angle| < 10°).
             *
             * The sound only plays when the axe is on-screen.  Volume is
             * set per-channel immediately after Mix_PlayChannel returns,
             * using the distance-based value computed above.
             */
            if (fabsf(t->angle) > AXE_SWING_AMPLITUDE * 0.95f) {
                if (!t->sound_played) {
                    if (snd_axe && on_screen && vol > 0) {
                        int ch = Mix_PlayChannel(-1, snd_axe, 0);
                        if (ch >= 0) Mix_Volume(ch, vol);
                    }
                    t->sound_played = 1;
                }
            } else if (fabsf(t->angle) < 10.0f) {
                /* Reset the sound flag as the axe crosses the vertical centre */
                t->sound_played = 0;
            }

        } else {  /* AXE_MODE_SPIN */
            /*
             * Continuous rotation — clockwise at a constant rate.
             * fmodf wraps the angle back to [0, 360) to avoid float drift.
             */
            t->angle += AXE_SPIN_SPEED * dt;

            /*
             * Sound trigger — fire once each time the angle wraps past 360°.
             * Volume scales with player distance, same as pendulum mode.
             */
            if (t->angle >= 360.0f) {
                if (snd_axe && on_screen && vol > 0) {
                    int ch = Mix_PlayChannel(-1, snd_axe, 0);
                    if (ch >= 0) Mix_Volume(ch, vol);
                }
            }

            t->angle = fmodf(t->angle, 360.0f);
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * axe_traps_render — Draw each active axe trap with rotation.
 *
 * SDL_RenderCopyEx is the rotated-blit variant of SDL_RenderCopy.
 * It takes a rotation angle (degrees, clockwise) and a pivot point
 * relative to the destination rect's top-left corner.
 *
 * The pivot is set to the top-centre of the sprite (the handle tip),
 * so the axe swings around that point exactly like a real pendulum.
 */
void axe_traps_render(const AxeTrap *traps, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x) {
    if (!tex) return;

    /*
     * src — the entire 48×64 sprite (single image, no sub-frames).
     * We blit the full texture each time.
     */
    SDL_Rect src = { 0, 0, AXE_FRAME_W, AXE_FRAME_H };

    for (int i = 0; i < count; i++) {
        const AxeTrap *t = &traps[i];
        if (!t->active) continue;

        /*
         * Off-screen culling — skip axes that are entirely outside the
         * visible viewport.  The sprite can swing AXE_DISPLAY_H pixels
         * from the pivot, so we add that as a margin on both sides.
         */
        if (t->x + AXE_DISPLAY_H < (float)cam_x ||
            t->x - AXE_DISPLAY_H > (float)(cam_x + GAME_W))
            continue;

        /*
         * dst — position the sprite so that the top-centre of the image
         * aligns with the trap's pivot point (t->x, t->y).
         *
         * The pivot is at the top-centre of the handle, so:
         *   dst.x = pivot_x − half_sprite_width
         *   dst.y = pivot_y  (handle top aligns with pivot)
         *
         * cam_x is subtracted to convert from world space to screen space.
         */
        SDL_Rect dst = {
            .x = (int)t->x - AXE_DISPLAY_W / 2 - cam_x,
            .y = (int)t->y,
            .w = AXE_DISPLAY_W,
            .h = AXE_DISPLAY_H,
        };

        /*
         * pivot — the rotation centre relative to dst's top-left corner.
         *
         * We want the axe to rotate around the top-centre of the handle.
         * In sprite coordinates that is (width/2, 0) — the middle of the
         * top edge.  A small offset of 4 px downward places the pivot
         * at the hook/nail point where the axe would be mounted.
         */
        SDL_Point pivot = {
            .x = AXE_DISPLAY_W / 2,  /* horizontal centre of the sprite */
            .y = 4,                   /* a few pixels below the very top */
        };

        /*
         * SDL_RenderCopyEx — blit with rotation.
         *   renderer  → GPU drawing context
         *   tex       → the Axe_Trap.png texture
         *   &src      → full source image (48×64)
         *   &dst      → where to draw on screen
         *   t->angle  → rotation in degrees (clockwise)
         *   &pivot    → the point around which to rotate (handle top)
         *   SDL_FLIP_NONE → no horizontal/vertical mirroring
         */
        SDL_RenderCopyEx(renderer, tex, &src, &dst,
                         (double)t->angle, &pivot, SDL_FLIP_NONE);
    }
}

/* ------------------------------------------------------------------ */

/*
 * axe_trap_get_hitbox — Approximate collision rect for the blade region.
 *
 * The blade occupies roughly the bottom 32×32 px of the 48×64 sprite.
 * We compute where that region ends up after rotation around the pivot.
 *
 * For simplicity we use a fixed 28×28 px hitbox centred on the blade tip
 * position.  The tip is AXE_DISPLAY_H − 4 px below the pivot (accounting
 * for the pivot offset), rotated by the current angle.
 *
 * Blade-tip position in world space:
 *   tip_x = pivot_x + arm_length × sin(angle_radians)
 *   tip_y = pivot_y + arm_length × cos(angle_radians)
 *
 * (sin for x, cos for y because angle 0 = straight down in our convention)
 */
SDL_Rect axe_trap_get_hitbox(const AxeTrap *trap) {
    /* Distance from pivot to blade centre (approximately 3/4 of sprite height) */
    float arm = (float)(AXE_DISPLAY_H - 4) * 0.70f;

    float rad = trap->angle * (float)M_PI / 180.0f;

    /*
     * Rotate the blade centre point around the pivot.
     *
     * SDL_RenderCopyEx rotates clockwise: at +90° the blade (bottom of the
     * sprite) points LEFT in screen space.  Standard sinf(+90°) returns +1
     * (rightward), so we negate the sin term to match SDL's visual rotation.
     * cosf is correct as-is: at 0° the blade is straight down (max cos),
     * and at ±90° it is level with the pivot (cos = 0).
     */
    float blade_cx = trap->x - arm * sinf(rad);
    float blade_cy = trap->y + 4.0f + arm * cosf(rad);

    /* Return a 28×28 hitbox centred on the blade */
    int hw = 14;
    int hh = 14;
    SDL_Rect hit = {
        .x = (int)(blade_cx - hw),
        .y = (int)(blade_cy - hh),
        .w = hw * 2,
        .h = hh * 2,
    };
    return hit;
}
