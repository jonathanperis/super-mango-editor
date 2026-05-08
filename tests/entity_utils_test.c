#include <stdio.h>

#include "core/entity_utils.h"

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "entity_utils_test: %s got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "entity_utils_test: %s got %.3f expected %.3f\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int advances_animation_and_preserves_leftover_time(void)
{
    int frame = 0;
    Uint32 timer = 0;

    if (expect_int("first advance wrap", animate_frame_ms(&frame, &timer, 0.120f, 100, 3), 0) != 0)
        return 1;
    if (expect_int("first frame", frame, 1) != 0) return 1;
    if (expect_int("leftover ms", (int)timer, 20) != 0) return 1;

    if (expect_int("second advance wrap", animate_frame_ms(&frame, &timer, 0.100f, 100, 3), 0) != 0)
        return 1;
    if (expect_int("second frame", frame, 2) != 0) return 1;

    if (expect_int("third advance wrap", animate_frame_ms(&frame, &timer, 0.100f, 100, 3), 1) != 0)
        return 1;
    if (expect_int("wrapped frame", frame, 0) != 0) return 1;

    return 0;
}

static int reverses_patrol_at_boundaries(void)
{
    float x = 88.0f;
    float vx = 50.0f;

    patrol_update(&x, &vx, 16.0f, 20.0f, 100.0f, 50.0f, 0.25f);
    if (expect_float("right snap x", x, 84.0f) != 0) return 1;
    if (expect_float("right reverse vx", vx, -50.0f) != 0) return 1;

    x = 22.0f;
    vx = -50.0f;
    patrol_update(&x, &vx, 16.0f, 20.0f, 100.0f, 50.0f, 0.25f);
    if (expect_float("left snap x", x, 20.0f) != 0) return 1;
    if (expect_float("left reverse vx", vx, 50.0f) != 0) return 1;

    return 0;
}

static int reverses_at_floor_gaps(void)
{
    int gaps[] = {100};
    float x = 80.0f;
    float vx = 40.0f;

    patrol_gap_reverse(&x, &vx, 0.0f, 48.0f, 40.0f, gaps, 1, 32);
    if (expect_float("right gap snap x", x, 52.0f) != 0) return 1;
    if (expect_float("right gap reverse vx", vx, -40.0f) != 0) return 1;

    x = 120.0f;
    vx = -40.0f;
    patrol_gap_reverse(&x, &vx, 0.0f, 16.0f, 40.0f, gaps, 1, 32);
    if (expect_float("left gap snap x", x, 132.0f) != 0) return 1;
    if (expect_float("left gap reverse vx", vx, 40.0f) != 0) return 1;

    return 0;
}

static int computes_sound_falloff(void)
{
    if (expect_int("near volume", sound_volume_for_distance(0.0f, 400.0f, 128), 128) != 0)
        return 1;
    if (expect_int("mid volume", sound_volume_for_distance(200.0f, 400.0f, 128), 64) != 0)
        return 1;
    if (expect_int("far volume", sound_volume_for_distance(400.0f, 400.0f, 128), 0) != 0)
        return 1;

    return 0;
}

int main(void)
{
    if (advances_animation_and_preserves_leftover_time() != 0) return 1;
    if (reverses_patrol_at_boundaries() != 0) return 1;
    if (reverses_at_floor_gaps() != 0) return 1;
    if (computes_sound_falloff() != 0) return 1;

    puts("entity_utils_test: ok");
    return 0;
}
