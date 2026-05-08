#include <stdio.h>

#include <SDL.h>

#include "hazards/spike_platform.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "collision_test: %s got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int sdl_intersection_edges_are_strict(void)
{
    SDL_Rect a = {10, 10, 16, 16};
    SDL_Rect touching = {26, 10, 16, 16};
    SDL_Rect overlapping = {25, 10, 16, 16};

    if (expect_int("touching edge", SDL_HasIntersection(&a, &touching), SDL_FALSE) != 0)
        return 1;
    if (expect_int("one-pixel overlap", SDL_HasIntersection(&a, &overlapping), SDL_TRUE) != 0)
        return 1;

    return 0;
}

static int spike_platform_hitbox_extends_upward(void)
{
    SpikePlatform sp = { .x = 120.0f, .y = 200.0f, .w = 48, .active = 1 };
    SDL_Rect hitbox = spike_platform_get_rect(&sp);
    SDL_Rect edge_aligned_player = {120, 187, 16, 11};

    if (expect_int("spike x", hitbox.x, 120) != 0) return 1;
    if (expect_int("spike y", hitbox.y, 198) != 0) return 1;
    if (expect_int("spike w", hitbox.w, 48) != 0) return 1;
    if (expect_int("spike h", hitbox.h, SPIKE_PLAT_SRC_H + 2) != 0) return 1;
    if (expect_int("standing overlap", SDL_HasIntersection(&edge_aligned_player, &hitbox), SDL_FALSE) != 0)
        return 1;

    edge_aligned_player.y = 188;
    if (expect_int("extended top overlap", SDL_HasIntersection(&edge_aligned_player, &hitbox), SDL_TRUE) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (sdl_intersection_edges_are_strict() != 0) return 1;
    if (spike_platform_hitbox_extends_upward() != 0) return 1;

    puts("collision_test: ok");
    return 0;
}
