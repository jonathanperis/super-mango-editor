/*
 * faster_fish.c — FasterFish: a fast variant of the jumping fish enemy.
 *
 * Same mechanics as the regular Fish but with higher speed (120 px/s),
 * a stronger jump impulse (-420 px/s), and shorter delay between jumps.
 * The behaviour lives once, in fish.c; this file only supplies the tuning.
 */

#include "faster_fish.h"

static const FishSpec s_faster_fish = {
    FFISH_JUMP_VY,
    FFISH_JUMP_MIN,
    FFISH_JUMP_MAX,
    FFISH_FRAME_MS
};

/* ------------------------------------------------------------------ */

void faster_fish_update(FasterFish *fish, int count, float dt, int world_w)
{
    fish_variant_update(&s_faster_fish, fish, count, dt, world_w);
}

void faster_fish_render(const FasterFish *fish, int count,
                        Texture2D *tex, int cam_x)
{
    fish_render(fish, count, tex, cam_x);
}

IntRect faster_fish_get_hitbox(const FasterFish *fish)
{
    return fish_get_hitbox(fish);
}
