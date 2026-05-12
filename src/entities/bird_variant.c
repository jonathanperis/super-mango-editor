/*
 * bird_variant.c — Shared bird variant movement, audio, hitbox, and render code.
 */

#include "bird_variant.h"

#include <math.h>  /* fabsf, sinf */

#include "bird.h"
#include "faster_bird.h"
#include "../core/entity_utils.h"
#include "../game.h"

#define BIRD_VARIANT_VOL_MAX 67

static const BirdVariantSpec s_bird_variants[BIRD_VARIANT_COUNT] = {
    [BIRD_VARIANT_REGULAR] = {
        BIRD_FRAMES,
        BIRD_FRAME_W,
        BIRD_ART_X,
        BIRD_ART_Y,
        BIRD_ART_W,
        BIRD_ART_H,
        BIRD_FRAME_MS,
        BIRD_SPEED,
        BIRD_WAVE_AMP,
        BIRD_WAVE_FREQ,
        (float)GAME_W,
        BIRD_VARIANT_VOL_MAX
    },
    [BIRD_VARIANT_FAST] = {
        FBIRD_FRAMES,
        FBIRD_FRAME_W,
        FBIRD_ART_X,
        FBIRD_ART_Y,
        FBIRD_ART_W,
        FBIRD_ART_H,
        FBIRD_FRAME_MS,
        FBIRD_SPEED,
        FBIRD_WAVE_AMP,
        FBIRD_WAVE_FREQ,
        (float)GAME_W,
        BIRD_VARIANT_VOL_MAX
    }
};

const BirdVariantSpec *bird_variant_spec(BirdVariantKind kind)
{
    if (kind < 0 || kind >= BIRD_VARIANT_COUNT) {
        return &s_bird_variants[BIRD_VARIANT_REGULAR];
    }
    return &s_bird_variants[kind];
}

void bird_variant_update(const BirdVariantSpec *spec,
                         float *x, float *vx,
                         float patrol_x0, float patrol_x1,
                         int *frame_index, Uint32 *anim_timer_ms,
                         float dt, Mix_Chunk *snd_flap,
                         float player_x, int cam_x)
{
    int wrapped;
    float bird_cx;
    int on_screen;

    if (!spec) spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    patrol_update(x, vx, (float)spec->frame_w,
                  patrol_x0, patrol_x1, spec->speed, dt);

    wrapped = animate_frame_ms(frame_index, anim_timer_ms,
                               dt, spec->frame_ms, spec->frames);
    if (!wrapped || !snd_flap) return;

    bird_cx = *x + (float)spec->frame_w / 2.0f;
    on_screen = (bird_cx >= (float)cam_x - (float)spec->frame_w &&
                 bird_cx <= (float)cam_x + GAME_W + (float)spec->frame_w);
    if (on_screen) {
        float dist = fabsf(player_x - bird_cx);
        int vol = sound_volume_for_distance(dist, spec->audible_range,
                                            spec->volume_max);
        if (vol > 0) {
            int ch = Mix_PlayChannel(-1, snd_flap, 0);
            if (ch >= 0) Mix_Volume(ch, vol);
        }
    }
}

float bird_variant_screen_y(const BirdVariantSpec *spec, float x, float base_y)
{
    if (!spec) spec = bird_variant_spec(BIRD_VARIANT_REGULAR);
    return base_y + sinf(x * spec->wave_freq) * spec->wave_amp;
}

SDL_Rect bird_variant_hitbox(const BirdVariantSpec *spec, float x, float base_y)
{
    float sy;
    SDL_Rect r;

    if (!spec) spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    sy = bird_variant_screen_y(spec, x, base_y);
    r.x = (int)x + spec->art_x;
    r.y = (int)sy;
    r.w = spec->art_w;
    r.h = spec->art_h;
    return r;
}

void bird_variant_render(const BirdVariantSpec *spec,
                         float x, float base_y, float vx, int frame_index,
                         SDL_Renderer *renderer, SDL_Texture *tex, int cam_x)
{
    float sy;
    SDL_Rect src;
    SDL_Rect dst;
    SDL_RendererFlip flip;

    if (!spec) spec = bird_variant_spec(BIRD_VARIANT_REGULAR);

    sy = bird_variant_screen_y(spec, x, base_y);
    src.x = frame_index * spec->frame_w;
    src.y = spec->art_y;
    src.w = spec->frame_w;
    src.h = spec->art_h;

    dst.x = (int)x - cam_x;
    dst.y = (int)sy;
    dst.w = spec->frame_w;
    dst.h = spec->art_h;

    flip = (vx > 0.0f) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, tex, &src, &dst, 0.0, NULL, flip);
}
