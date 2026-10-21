/*
 * creature_update_test.c — Direct checks of the bird and spider updates.
 *
 * gameplay_mechanics_test.c watches these creatures inside a whole level.
 * Here each update function runs on a few hand-made structs, with no level
 * and no GameState, so every rule is checked against the real constants
 * from bird.h, faster_bird.h and spider.h: how far one step moves, where a
 * patrol turns, how fast the wings beat, the height of the sine wave, and
 * where a spider stops at a floor gap.
 *
 * Linked into gameplay-mechanics-test, whose main() runs these cases.
 * Every call passes NULL for the flap sound: what a bird does must not
 * depend on whether its sound loaded.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/game_timing.h"     /* GAME_FIXED_STEP */
#include "entities/bird.h"
#include "entities/faster_bird.h"
#include "entities/spider.h"
#include "game_constants.h"       /* FLOOR_GAP_W, GAME_W */
#include "gameplay_mechanics_test.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "creature_update_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)
#define NEAR(a, b, tolerance) (fabsf((float)(a) - (float)(b)) <= (tolerance))

/* A bird flying right from x inside the patrol [x0, x1]. */
static Bird make_bird(float x, float vx, float x0, float x1)
{
    Bird b;
    memset(&b, 0, sizeof(b));
    b.x = x;
    b.base_y = 60.0f;
    b.vx = vx;
    b.patrol_x0 = x0;
    b.patrol_x1 = x1;
    return b;
}

static Spider make_spider(float x, float vx, float x0, float x1)
{
    Spider s;
    memset(&s, 0, sizeof(s));
    s.x = x;
    s.vx = vx;
    s.patrol_x0 = x0;
    s.patrol_x1 = x1;
    return s;
}

/* ------------------------------------------------------------------ */
/* Birds                                                               */
/* ------------------------------------------------------------------ */

/*
 * Each step moves a bird by vx × dt, and it turns where its frame meets a
 * patrol end: the right edge of the 48 px frame at patrol_x1, the left
 * edge at patrol_x0. A turn flips the direction and keeps the speed.
 */
int birds_update_patrols_and_turns(void)
{
    int failed = 0;
    Bird b = make_bird(100.0f, BIRD_SPEED, 50.0f, 250.0f);

    birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
    CHECK(NEAR(b.x, 100.0f + BIRD_SPEED / 60.0f, 0.001f));
    CHECK(b.vx == BIRD_SPEED);

    /* One second later: 45 px further, still flying right. */
    for (int i = 0; i < 59; i++) birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
    CHECK(NEAR(b.x, 100.0f + BIRD_SPEED, 0.01f));

    /* Close to the right end, the next step snaps the frame to it and turns. */
    b.x = 250.0f - BIRD_FRAME_W - 0.5f;
    birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
    CHECK(b.x == 250.0f - BIRD_FRAME_W);
    CHECK(b.vx == -BIRD_SPEED);
    birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
    CHECK(b.x < 250.0f - BIRD_FRAME_W);

    /* The same at the left end, where the left edge is what counts. */
    b.x = 50.5f;
    birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
    CHECK(b.x == 50.0f && b.vx == BIRD_SPEED);

    /* An authored speed is kept through every turn, and over a long flight
     * the frame never leaves the patrol. */
    b = make_bird(60.0f, -120.0f, 50.0f, 250.0f);
    int turns = 0;
    for (int i = 0; i < 20 * 60; i++) {
        float before = b.vx;
        birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
        if ((before > 0.0f) != (b.vx > 0.0f)) turns++;
        CHECK(fabsf(b.vx) == 120.0f);
        CHECK(b.x >= 50.0f && b.x + BIRD_FRAME_W <= 250.0f);
    }
    /* 20 s at 120 px/s over a 152 px track: about 15 turns. */
    CHECK(turns >= 14 && turns <= 17);
done:
    return failed;
}

/*
 * Wings beat one frame every BIRD_FRAME_MS (140 ms): 8 fixed steps
 * (133 ms) is not enough, the 9th (150 ms) is. The faster bird holds a
 * frame only FBIRD_FRAME_MS (90 ms), so it is on its second frame after 6
 * steps. Both wrap back to frame 0 after BIRD_FRAMES frames, and the time
 * left over carries into the next frame instead of being dropped.
 */
int bird_wings_beat_at_each_variant_rate(void)
{
    int failed = 0;
    Bird slow = make_bird(100.0f, BIRD_SPEED, 0.0f, 400.0f);
    FasterBird fast = make_bird(100.0f, FBIRD_SPEED, 0.0f, 400.0f);

    for (int step = 1; step <= 9; step++) {
        birds_update(&slow, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
        faster_birds_update(&fast, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
        if (step == 5) CHECK(slow.frame_index == 0 && fast.frame_index == 0);
        if (step == 6) CHECK(slow.frame_index == 0 && fast.frame_index == 1);
        if (step == 8) CHECK(slow.frame_index == 0);
        if (step == 9) CHECK(slow.frame_index == 1);
    }
    /* 150 ms played, one 140 ms frame shown: 10 ms carried over. */
    CHECK(NEAR(slow.anim_timer_ms, 9 * 1000.0f / 60.0f - BIRD_FRAME_MS, 0.01f));

    /* A whole wing beat brings the frame back to 0. */
    slow.frame_index = 0;
    slow.anim_timer_ms = 0.0f;
    for (int frame = 1; frame <= BIRD_FRAMES; frame++) {
        birds_update(&slow, 1, BIRD_FRAME_MS / 1000.0f + 0.001f, NULL, 0.0f, 0);
        CHECK(slow.frame_index == frame % BIRD_FRAMES);
    }
done:
    return failed;
}

/*
 * The hitbox is the visible art (15×14 px, 17 px into the frame), and its
 * top follows base_y + sin(x × freq) × amp. At a crest the bird is a whole
 * amplitude below base_y (y grows downwards): 20 px for the regular bird,
 * 15 px for the faster one, whose wave is also shorter.
 */
int bird_hitbox_rides_the_sine_wave(void)
{
    int failed = 0;
    const float half_pi = 1.5707963f;
    Bird b = make_bird(0.0f, BIRD_SPEED, 0.0f, 1000.0f);
    b.base_y = 100.5f;  /* half a pixel, so truncating a whole result is safe */

    IntRect hit = bird_get_hitbox(&b);           /* sin(0) = 0 */
    CHECK(hit.x == BIRD_ART_X && hit.y == 100);
    CHECK(hit.w == BIRD_ART_W && hit.h == BIRD_ART_H);

    b.x = half_pi / BIRD_WAVE_FREQ;              /* crest */
    hit = bird_get_hitbox(&b);
    CHECK(hit.x == (int)b.x + BIRD_ART_X);
    CHECK(hit.y == (int)(100.5f + BIRD_WAVE_AMP));
    b.x = 3.0f * half_pi / BIRD_WAVE_FREQ;       /* trough */
    CHECK(bird_get_hitbox(&b).y == (int)(100.5f - BIRD_WAVE_AMP));

    FasterBird f = b;
    f.x = half_pi / FBIRD_WAVE_FREQ;
    hit = faster_bird_get_hitbox(&f);
    CHECK(hit.y == (int)(100.5f + FBIRD_WAVE_AMP));
    CHECK(hit.w == FBIRD_ART_W && hit.h == FBIRD_ART_H);

    /* Over a long flight the hitbox stays within one amplitude of base_y. */
    int min_y = 1000, max_y = -1000;
    for (int i = 0; i < 10 * 60; i++) {
        birds_update(&b, 1, GAME_FIXED_STEP, NULL, 0.0f, 0);
        hit = bird_get_hitbox(&b);
        if (hit.y < min_y) min_y = hit.y;
        if (hit.y > max_y) max_y = hit.y;
    }
    CHECK(min_y >= (int)(100.5f - BIRD_WAVE_AMP) && max_y <= (int)(100.5f + BIRD_WAVE_AMP));
    CHECK(max_y - min_y >= 2 * (int)BIRD_WAVE_AMP - 1);
done:
    return failed;
}

/*
 * bird_variant_update takes the tuning as a BirdSpec, so a variant can
 * change the frame width the patrol turns on and the number of frames.
 * It only touches the first `count` birds of the array.
 */
int bird_variant_uses_its_spec(void)
{
    int failed = 0;
    const BirdSpec wide = {
        2,        /* frames */
        64,       /* frame_w */
        0, 0, 64, 16,
        100,      /* frame_ms */
        10.0f,    /* wave_amp */
        0.01f     /* wave_freq */
    };
    Bird birds[2] = {
        make_bird(200.0f - 64.0f - 0.5f, 60.0f, 0.0f, 200.0f),
        make_bird(10.0f, 60.0f, 0.0f, 200.0f),
    };

    /* One 110 ms step: the 64 px frame meets patrol_x1 and turns; one
     * frame shown out of two. */
    bird_variant_update(&wide, birds, 1, 0.11f, NULL, 0.0f, 0);
    CHECK(birds[0].x == 200.0f - 64.0f && birds[0].vx == -60.0f);
    CHECK(birds[0].frame_index == 1);
    bird_variant_update(&wide, birds, 1, 0.11f, NULL, 0.0f, 0);
    CHECK(birds[0].frame_index == 0);            /* wrapped after 2 frames */

    /* The second bird was outside count: untouched. */
    CHECK(birds[1].x == 10.0f && birds[1].frame_index == 0 && birds[1].anim_timer_ms == 0.0f);

    /* No time passing moves nothing and shows no new frame. */
    Bird still = birds[0];
    bird_variant_update(&wide, birds, 2, 0.0f, NULL, 0.0f, 0);
    CHECK(birds[0].x == still.x && birds[0].frame_index == still.frame_index);
    CHECK(birds[1].x == 10.0f);

    /* The variant's hitbox uses the spec's art box and wave too. */
    birds[1].base_y = 40.5f;
    birds[1].x = 1.5707963f / wide.wave_freq;
    IntRect hit = bird_variant_hitbox(&wide, &birds[1]);
    CHECK(hit.x == (int)birds[1].x && hit.w == 64 && hit.h == 16);
    CHECK(hit.y == (int)(40.5f + wide.wave_amp));
done:
    return failed;
}

/* ------------------------------------------------------------------ */
/* Spiders                                                             */
/* ------------------------------------------------------------------ */

/*
 * A spider walks vx × dt per step and turns at its patrol ends exactly as
 * a bird does, measured on its 64 px frame, keeping its authored speed.
 * Its legs move one frame every SPIDER_FRAME_MS (150 ms).
 */
int spiders_update_patrols_and_animates(void)
{
    int failed = 0;
    Spider s = make_spider(100.0f, SPIDER_SPEED, 40.0f, 300.0f);

    spiders_update(&s, 1, GAME_FIXED_STEP, NULL, 0);
    CHECK(NEAR(s.x, 100.0f + SPIDER_SPEED / 60.0f, 0.001f));

    s.x = 300.0f - SPIDER_FRAME_W - 0.25f;
    spiders_update(&s, 1, GAME_FIXED_STEP, NULL, 0);
    CHECK(s.x == 300.0f - SPIDER_FRAME_W && s.vx == -SPIDER_SPEED);
    s.x = 40.25f;
    spiders_update(&s, 1, GAME_FIXED_STEP, NULL, 0);
    CHECK(s.x == 40.0f && s.vx == SPIDER_SPEED);

    /* Legs: 8 steps (133 ms) still frame 0, 10 steps (167 ms) frame 1. */
    s = make_spider(100.0f, SPIDER_SPEED, 40.0f, 300.0f);
    for (int i = 0; i < 8; i++) spiders_update(&s, 1, GAME_FIXED_STEP, NULL, 0);
    CHECK(s.frame_index == 0);
    for (int i = 0; i < 2; i++) spiders_update(&s, 1, GAME_FIXED_STEP, NULL, 0);
    CHECK(s.frame_index == 1);
    /* Three frames of legs, then back to the first. */
    s.frame_index = 0;
    s.anim_timer_ms = 0.0f;
    for (int frame = 1; frame <= SPIDER_FRAMES; frame++) {
        spiders_update(&s, 1, SPIDER_FRAME_MS / 1000.0f + 0.001f, NULL, 0);
        CHECK(s.frame_index == frame % SPIDER_FRAMES);
    }

    /* A fast authored spider keeps its speed and its patrol over time. */
    s = make_spider(200.0f, -150.0f, 40.0f, 300.0f);
    for (int i = 0; i < 10 * 60; i++) {
        spiders_update(&s, 1, GAME_FIXED_STEP, NULL, 0);
        CHECK(fabsf(s.vx) == 150.0f);
        CHECK(s.x >= 40.0f && s.x + SPIDER_FRAME_W <= 300.0f);
    }
done:
    return failed;
}

/*
 * A spider never walks over a floor gap. When the middle of its visible
 * art (SPIDER_ART_X + SPIDER_ART_W / 2 into the frame) would be over the
 * hole, it is put back with its art touching the gap edge and turned.
 * Walking right, the art's right edge stops at the gap's left edge;
 * walking left, the art's left edge stops at the gap's right edge.
 */
int spiders_turn_at_floor_gaps(void)
{
    int failed = 0;
    const int gaps[2] = {160, 400};
    const float centre = SPIDER_ART_X + SPIDER_ART_W / 2.0f;

    /* Walking right, the art centre one step short of the first gap. */
    Spider s = make_spider(160.0f - centre - 0.5f, SPIDER_SPEED, 0.0f, 600.0f);
    spiders_update(&s, 1, GAME_FIXED_STEP, gaps, 2);
    CHECK(s.x == 160.0f - SPIDER_ART_X - SPIDER_ART_W);
    CHECK(s.vx == -SPIDER_SPEED);

    /* Walking left into the second gap from its right side. */
    s = make_spider(400.0f + FLOOR_GAP_W - centre + 0.5f, -SPIDER_SPEED, 0.0f, 600.0f);
    spiders_update(&s, 1, GAME_FIXED_STEP, gaps, 2);
    CHECK(s.x == 400.0f + FLOOR_GAP_W - SPIDER_ART_X);
    CHECK(s.vx == SPIDER_SPEED);

    /* With the gap list empty, the same spider walks on over the spot. */
    s = make_spider(160.0f - centre - 0.5f, SPIDER_SPEED, 0.0f, 600.0f);
    spiders_update(&s, 1, GAME_FIXED_STEP, gaps, 0);
    CHECK(s.vx == SPIDER_SPEED && s.x > 160.0f - centre);

    /* Between the two gaps it walks back and forth for good, its art
     * centre never over either hole. */
    s = make_spider(250.0f, SPIDER_SPEED, 0.0f, 600.0f);
    int turns = 0;
    for (int i = 0; i < 20 * 60; i++) {
        float before = s.vx;
        spiders_update(&s, 1, GAME_FIXED_STEP, gaps, 2);
        if ((before > 0.0f) != (s.vx > 0.0f)) turns++;
        float c = s.x + centre;
        CHECK(c >= 160.0f + FLOOR_GAP_W && c < 400.0f);
    }
    CHECK(turns >= 4);  /* 1000 px walked over a 208 px stretch */
done:
    return failed;
}
