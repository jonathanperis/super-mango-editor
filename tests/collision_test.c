#include <stdio.h>

#include "shared/geometry.h"

#include "entities/fish.h"
#include "hazards/circular_saw.h"
#include "hazards/spike_platform.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "collision_test: %s got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int integer_intersection_edges_are_strict(void)
{
    IntRect a = {10, 10, 16, 16};
    IntRect touching = {26, 10, 16, 16};
    IntRect overlapping = {25, 10, 16, 16};

    if (expect_int("touching edge", rect_intersects(&a, &touching), 0) != 0)
        return 1;
    if (expect_int("one-pixel overlap", rect_intersects(&a, &overlapping), 1) != 0)
        return 1;

    return 0;
}

static int spike_platform_hitbox_extends_upward(void)
{
    SpikePlatform sp = { .x = 120.0f, .y = 200.0f, .w = 48, .active = 1 };
    IntRect hitbox = spike_platform_get_rect(&sp);
    IntRect edge_aligned_player = {120, 187, 16, 11};

    if (expect_int("spike x", hitbox.x, 120) != 0) return 1;
    if (expect_int("spike y", hitbox.y, 198) != 0) return 1;
    if (expect_int("spike w", hitbox.w, 48) != 0) return 1;
    if (expect_int("spike h", hitbox.h, SPIKE_PLAT_SRC_H + 2) != 0) return 1;
    if (expect_int("standing overlap", rect_intersects(&edge_aligned_player, &hitbox), 0) != 0)
        return 1;

    edge_aligned_player.y = 188;
    if (expect_int("extended top overlap", rect_intersects(&edge_aligned_player, &hitbox), 1) != 0)
        return 1;

    return 0;
}

static int fish_hitbox_trims_transparent_padding(void)
{
    Fish fish = { .x = 100.0f, .y = 200.0f };
    IntRect hitbox = fish_get_hitbox(&fish);

    if (expect_int("fish hitbox x", hitbox.x, 100 + FISH_HITBOX_PAD_X) != 0)
        return 1;
    if (expect_int("fish hitbox y", hitbox.y, 200 + FISH_HITBOX_PAD_Y) != 0)
        return 1;
    if (expect_int("fish hitbox w", hitbox.w,
                   FISH_RENDER_W - 2 * FISH_HITBOX_PAD_X) != 0)
        return 1;
    if (expect_int("fish hitbox h", hitbox.h,
                   FISH_RENDER_H - FISH_HITBOX_PAD_Y - 16) != 0)
        return 1;

    return 0;
}

static int circular_saw_hitbox_trims_corners(void)
{
    CircularSaw saw = {
        .x = 240.0f,
        .y = 160.0f,
        .w = SAW_DISPLAY_W,
        .h = SAW_DISPLAY_H,
        .active = 1
    };
    IntRect hitbox = circular_saw_get_hitbox(&saw);

    if (expect_int("saw hitbox x", hitbox.x, 244) != 0) return 1;
    if (expect_int("saw hitbox y", hitbox.y, 164) != 0) return 1;
    if (expect_int("saw hitbox w", hitbox.w, SAW_DISPLAY_W - 8) != 0) return 1;
    if (expect_int("saw hitbox h", hitbox.h, SAW_DISPLAY_H - 8) != 0) return 1;

    return 0;
}

int main(void)
{
    if (integer_intersection_edges_are_strict() != 0) return 1;
    if (spike_platform_hitbox_extends_upward() != 0) return 1;
    if (fish_hitbox_trims_transparent_padding() != 0) return 1;
    if (circular_saw_hitbox_trims_corners() != 0) return 1;

    puts("collision_test: ok");
    return 0;
}
