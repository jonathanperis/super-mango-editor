/*
 * bird_variant.h — Shared metadata and helpers for bird enemy variants.
 */
#pragma once

#include "../shared/graphics.h"
#include "../shared/audio.h"

typedef enum {
    BIRD_VARIANT_REGULAR = 0,
    BIRD_VARIANT_FAST,
    BIRD_VARIANT_COUNT
} BirdVariantKind;

typedef struct {
    int    frames;
    int    frame_w;
    int    art_x;
    int    art_y;
    int    art_w;
    int    art_h;
    uint32_t frame_ms;
    float  speed;
    float  wave_amp;
    float  wave_freq;
    float  audible_range;
    int    volume_max;
} BirdVariantSpec;

const BirdVariantSpec *bird_variant_spec(BirdVariantKind kind);

void bird_variant_update(const BirdVariantSpec *spec,
                         float *x, float *vx,
                         float patrol_x0, float patrol_x1,
                         int *frame_index, uint32_t *anim_timer_ms,
                         float dt, SoundEffect *snd_flap,
                         float player_x, int cam_x);

float bird_variant_screen_y(const BirdVariantSpec *spec, float x, float base_y);

IntRect bird_variant_hitbox(const BirdVariantSpec *spec, float x, float base_y);

void bird_variant_render(const BirdVariantSpec *spec,
                         float x, float base_y, float vx, int frame_index,
                         Texture2D *tex, int cam_x);
