/*
 * player_climb_test.c — Direct checks of vine, ladder and rope climbing.
 *
 * gameplay_mechanics_test.c climbs the fixture level as a player would.
 * Here the climbing code in player/player_climb.c (and the climbing half
 * of player_handle_input) runs on one Player and a few hand-placed
 * climbables, with no level loaded, so each rule is checked on its own:
 * the size of each grab zone, which climbable wins when several overlap,
 * the 80 px/s climb, the clamp at the top, letting go at the bottom or to
 * the side, jumping off, and the short pause after a hit.
 *
 * Linked into gameplay-mechanics-test, whose main() runs these cases.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>  /* abs */
#include <string.h>

#include "core/game_timing.h"       /* GAME_FIXED_STEP */
#include "player/player.h"
#include "player/player_climb.h"
#include "player/player_internal.h" /* PLAYER_PHYS_PAD_X/TOP, PLAYER_FLOOR_SINK */
#include "gameplay_mechanics_test.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "player_climb_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)
#define NEAR(a, b, tolerance) (fabsf((float)(a) - (float)(b)) <= (tolerance))

/* player_input.c climbs and drifts at these speeds (px/s). */
#define CLIMB_SPEED 80.0f
/* Extra pixels each side of a climbable that still count as touching it. */
#define GRAB_PAD 4
#define TEST_WORLD_W 800

/*
 * A standing player whose hitbox (18×14 px, PLAYER_PHYS_PAD_X in from the
 * 48 px frame) is centred on centre_x, with its top at hitbox_top.
 */
static Player player_at(float centre_x, float hitbox_top)
{
    Player p;
    memset(&p, 0, sizeof(p));
    p.w = PLAYER_FRAME_W;
    p.h = PLAYER_FRAME_H;
    p.x = centre_x - PLAYER_FRAME_W / 2.0f;
    p.y = hitbox_top - PLAYER_PHYS_PAD_TOP;
    p.on_ground = 1;
    return p;
}

/* The climbing half of a game step: input, then climb-only movement. */
static int climb_step(Player *p, unsigned int input,
                      const VineDecor *vines, int vine_count,
                      const LadderDecor *ladders, int ladder_count,
                      const RopeDecor *ropes, int rope_count)
{
    player_handle_input(p, NULL, input, 0, vines, vine_count,
                        ladders, ladder_count, ropes, rope_count);
    return player_update_climbing(p, GAME_FIXED_STEP, vines, ladders, ropes, TEST_WORLD_W);
}

/* ------------------------------------------------------------------ */

/*
 * Each grab zone is the climbable's sprite widened by 4 px on both sides,
 * as tall as its stacked tiles: the first tile full height, every further
 * tile one step lower. player_climb_get_bounds reports the same zone, and
 * its top and bottom.
 */
int climb_grab_zones_cover_every_tile(void)
{
    int failed = 0;
    const VineDecor vine = {100.0f, 40.0f, 3, VINE_GREEN};
    const LadderDecor ladder = {200.0f, 60.0f, 5};
    const RopeDecor rope = {300.0f, 20.0f, 2};

    IntRect r = player_vine_grab_rect(&vine);
    CHECK(r.x == 100 - GRAB_PAD && r.y == 40);
    CHECK(r.w == VINE_W + 2 * GRAB_PAD && r.h == 2 * VINE_STEP + VINE_H);
    r = player_ladder_grab_rect(&ladder);
    CHECK(r.x == 200 - GRAB_PAD && r.y == 60);
    CHECK(r.w == LADDER_W + 2 * GRAB_PAD && r.h == 4 * LADDER_STEP + LADDER_H);
    r = player_rope_grab_rect(&rope);
    CHECK(r.x == 300 - GRAB_PAD && r.y == 20);
    CHECK(r.w == ROPE_W + 2 * GRAB_PAD && r.h == ROPE_STEP + ROPE_H);

    /* A single tile is exactly one sprite tall. */
    const VineDecor one = {0.0f, 0.0f, 1, VINE_BROWN};
    CHECK(player_vine_grab_rect(&one).h == VINE_H);

    /* The bounds follow climb_source and vine_index. */
    Player p = player_at(0.0f, 0.0f);
    IntRect grab;
    float top, bottom;
    p.climb_source = 0;
    p.vine_index = 0;
    player_climb_get_bounds(&p, &vine, &ladder, &rope, &grab, &top, &bottom);
    CHECK(grab.x == player_vine_grab_rect(&vine).x);
    CHECK(top == 40.0f && bottom == 40.0f + 2 * VINE_STEP + VINE_H);
    p.climb_source = 1;
    player_climb_get_bounds(&p, &vine, &ladder, &rope, &grab, &top, &bottom);
    CHECK(grab.x == 200 - GRAB_PAD);
    CHECK(top == 60.0f && bottom == 60.0f + 4 * LADDER_STEP + LADDER_H);
    p.climb_source = 2;
    player_climb_get_bounds(&p, &vine, &ladder, &rope, &grab, &top, &bottom);
    CHECK(grab.x == 300 - GRAB_PAD);
    CHECK(top == 20.0f && bottom == 20.0f + ROPE_STEP + ROPE_H);
done:
    return failed;
}

/*
 * player_try_grab_climbable starts a climb on the first climbable the
 * hitbox overlaps, checking vines, then ladders, then ropes. A grab stops
 * all movement and leaves the ground; a miss changes nothing.
 */
int climb_grab_picks_the_first_overlap(void)
{
    int failed = 0;
    /* A vine and a ladder in the same place, and two ropes further on. */
    const VineDecor vines[1] = {{100.0f, 40.0f, 4, VINE_GREEN}};
    const LadderDecor ladders[1] = {{100.0f, 40.0f, 6}};
    const RopeDecor ropes[2] = {{300.0f, 40.0f, 3}, {400.0f, 40.0f, 3}};

    Player p = player_at(108.0f, 60.0f);
    p.vx = 50.0f;
    p.vy = 20.0f;
    CHECK(player_try_grab_climbable(&p, vines, 1, ladders, 1, ropes, 2) == 1);
    CHECK(p.on_vine && p.climb_source == 0 && p.vine_index == 0);
    CHECK(!p.on_ground && p.vx == 0.0f && p.vy == 0.0f);

    /* Without the vine the ladder underneath is taken. */
    p = player_at(108.0f, 60.0f);
    CHECK(player_try_grab_climbable(&p, vines, 0, ladders, 1, ropes, 2) == 1);
    CHECK(p.climb_source == 1 && p.vine_index == 0);

    /* The index names which rope of the array is held. */
    p = player_at(408.0f, 60.0f);
    CHECK(player_try_grab_climbable(&p, vines, 1, ladders, 1, ropes, 2) == 1);
    CHECK(p.climb_source == 2 && p.vine_index == 1);

    /* The pad counts: a hitbox reaching 1 px into it grabs ... */
    const float half_body = (PLAYER_FRAME_W - 2 * PLAYER_PHYS_PAD_X) / 2.0f;
    p = player_at(300.0f - GRAB_PAD - half_body + 1.0f, 60.0f);
    CHECK(player_try_grab_climbable(&p, NULL, 0, NULL, 0, ropes, 1) == 1);
    /* ... one that only touches its edge does not, and keeps its state. */
    p = player_at(300.0f - GRAB_PAD - half_body, 60.0f);
    p.vx = 50.0f;
    CHECK(player_try_grab_climbable(&p, NULL, 0, NULL, 0, ropes, 1) == 0);
    CHECK(!p.on_vine && p.on_ground && p.vx == 50.0f);

    /* Below the bottom tile is out of reach too. */
    p = player_at(308.0f, 40.0f + ROPE_STEP * 2 + ROPE_H);
    CHECK(player_try_grab_climbable(&p, NULL, 0, NULL, 0, ropes, 1) == 0);
done:
    return failed;
}

/*
 * player_update_climbing does nothing for a player who is not climbing.
 * While climbing it moves by velocity × dt with no gravity, clamps the
 * head at the top of the climbable, lets go when the feet pass the bottom
 * or the hitbox drifts clear to the side, and keeps the body inside the
 * world. Hanging still freezes the climb animation.
 */
int climb_update_moves_clamps_and_lets_go(void)
{
    int failed = 0;
    const LadderDecor ladder = {100.0f, 40.0f, 6};   /* 40 .. 102 */
    const float bottom = 40.0f + 5 * LADDER_STEP + LADDER_H;

    Player p = player_at(108.0f, 70.0f);
    p.vy = -CLIMB_SPEED;
    CHECK(player_update_climbing(&p, GAME_FIXED_STEP, NULL, &ladder, NULL, TEST_WORLD_W) == 0);
    CHECK(p.y == 70.0f - PLAYER_PHYS_PAD_TOP);

    CHECK(player_try_grab_climbable(&p, NULL, 0, &ladder, 1, NULL, 0));
    p.vy = -CLIMB_SPEED;
    p.coyote_timer = 0.1f;
    p.jump_buffer_timer = 0.1f;
    float y = p.y;
    CHECK(player_update_climbing(&p, GAME_FIXED_STEP, NULL, &ladder, NULL, TEST_WORLD_W) == 1);
    CHECK(NEAR(y - p.y, CLIMB_SPEED / 60.0f, 0.001f));
    CHECK(p.coyote_timer == 0.0f && p.jump_buffer_timer == 0.0f);
    CHECK(p.anim_state == ANIM_CLIMB);

    /* Hanging still: no movement, and the climb frame stops changing. */
    p.vy = 0.0f;
    y = p.y;
    int frame = p.anim_frame_index;
    for (int i = 0; i < 30; i++)
        player_update_climbing(&p, GAME_FIXED_STEP, NULL, &ladder, NULL, TEST_WORLD_W);
    CHECK(p.on_vine && p.y == y && p.anim_frame_index == frame);

    /* Past the top the hitbox top stops at the ladder top. */
    p.y = 40.0f - PLAYER_PHYS_PAD_TOP + 0.5f;
    p.vy = -CLIMB_SPEED;
    player_update_climbing(&p, GAME_FIXED_STEP, NULL, &ladder, NULL, TEST_WORLD_W);
    CHECK(p.on_vine && p.y == 40.0f - PLAYER_PHYS_PAD_TOP && p.vy == 0.0f);

    /* Feet (y + h - PLAYER_FLOOR_SINK) passing the bottom let go. */
    p.y = bottom - (PLAYER_FRAME_H - PLAYER_FLOOR_SINK) - 0.5f;
    p.vy = CLIMB_SPEED;
    player_update_climbing(&p, GAME_FIXED_STEP, NULL, &ladder, NULL, TEST_WORLD_W);
    CHECK(!p.on_vine && p.vy == 0.0f);
    CHECK(p.anim_state != ANIM_CLIMB);

    /* Drifting clear of the grab zone sideways lets go as well. */
    p = player_at(108.0f, 70.0f);
    CHECK(player_try_grab_climbable(&p, NULL, 0, &ladder, 1, NULL, 0));
    p.vx = CLIMB_SPEED;
    int steps = 0;
    while (p.on_vine && steps < 60) {
        CHECK(player_update_climbing(&p, GAME_FIXED_STEP, NULL, &ladder, NULL, TEST_WORLD_W) == 1);
        steps++;
    }
    /* Clear once the hitbox's left edge (108 - 9 = 99) reaches the zone's
     * right edge (100 + 16 + 4 = 120): 21 px at 80 px/s, about 16 steps. */
    CHECK(!p.on_vine && p.vy == 0.0f);
    CHECK(NEAR(steps, (100 + LADDER_W + GRAB_PAD - (108.0f - 9.0f)) * 60.0f / CLIMB_SPEED, 1.5f));

    /* A climbable against the world edge: the body stops at x = 0. */
    const VineDecor edge = {0.0f, 40.0f, 4, VINE_GREEN};
    p = player_at(9.0f, 60.0f);                 /* hitbox 0 .. 18 */
    CHECK(player_try_grab_climbable(&p, &edge, 1, NULL, 0, NULL, 0));
    p.vx = -CLIMB_SPEED;
    for (int i = 0; i < 10; i++)
        player_update_climbing(&p, GAME_FIXED_STEP, &edge, NULL, NULL, TEST_WORLD_W);
    CHECK(p.on_vine && p.x == -(float)PLAYER_PHYS_PAD_X);
done:
    return failed;
}

/*
 * The climbing controls in player_handle_input: Up grabs a climbable in
 * reach; Up and Down climb at 80 px/s; Left and Right drift at 80 px/s and
 * turn the player; Jump lets go with a full jump. Up with Jump held never
 * grabs, and nor does Up in the first PLAYER_KNOCKBACK_GRAB_LOCK seconds
 * after a hit. Keys (the physical mask) work exactly like replay input.
 */
int climb_controls_follow_the_input(void)
{
    int failed = 0;
    const RopeDecor rope = {100.0f, 40.0f, 4};

    /* No Up: standing in reach does not grab. */
    Player p = player_at(108.0f, 70.0f);
    player_handle_input(&p, NULL, 0, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(!p.on_vine);
    /* Up grabs and starts climbing at once. */
    player_handle_input(&p, NULL, PLAYER_INPUT_UP, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(p.on_vine && p.climb_source == 2 && p.vy == -CLIMB_SPEED && p.vx == 0.0f);
    player_handle_input(&p, NULL, PLAYER_INPUT_DOWN, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(p.vy == CLIMB_SPEED);
    player_handle_input(&p, NULL, 0, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(p.on_vine && p.vx == 0.0f && p.vy == 0.0f);
    player_handle_input(&p, NULL, PLAYER_INPUT_LEFT, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(p.vx == -CLIMB_SPEED && p.facing_left);
    player_handle_input(&p, NULL, 0, PLAYER_INPUT_RIGHT | PLAYER_INPUT_UP,
                        NULL, 0, NULL, 0, &rope, 1);
    CHECK(p.vx == CLIMB_SPEED && !p.facing_left && p.vy == -CLIMB_SPEED);

    /* Jump lets go with the normal jump impulse. */
    player_handle_input(&p, NULL, PLAYER_INPUT_JUMP, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(!p.on_vine && p.vy == JUMP_VY && p.jump_held);

    /* Up together with Jump does not grab. */
    p = player_at(108.0f, 70.0f);
    player_handle_input(&p, NULL, PLAYER_INPUT_UP | PLAYER_INPUT_JUMP, 0,
                        NULL, 0, NULL, 0, &rope, 1);
    CHECK(!p.on_vine);

    /* Just hit: no grab during the lock, a grab once it has passed. */
    p = player_at(108.0f, 70.0f);
    p.hurt_timer = PLAYER_HURT_TIME;
    player_handle_input(&p, NULL, PLAYER_INPUT_UP, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(!p.on_vine);
    p.hurt_timer = PLAYER_HURT_TIME - PLAYER_KNOCKBACK_GRAB_LOCK - 0.01f;
    player_handle_input(&p, NULL, PLAYER_INPUT_UP, 0, NULL, 0, NULL, 0, &rope, 1);
    CHECK(p.on_vine);
done:
    return failed;
}

/*
 * The whole climb, step by step: Up from the bottom of a vine reaches the
 * top in height / 80 px/s and stays there; Down from the top lets go once
 * the feet pass the bottom tile.
 */
int climb_a_whole_vine_up_and_down(void)
{
    int failed = 0;
    const VineDecor vine = {100.0f, 40.0f, 5, VINE_GREEN};
    const float bottom = 40.0f + 4 * VINE_STEP + VINE_H;
    /* Start with the feet just above the bottom of the vine. */
    Player p = player_at(108.0f, 0.0f);
    p.y = bottom - (PLAYER_FRAME_H - PLAYER_FLOOR_SINK) - 1.0f;

    const float start_y = p.y;
    const float top_y = 40.0f - PLAYER_PHYS_PAD_TOP;
    int steps = 0;
    while (p.y > top_y && steps < 600) {
        CHECK(climb_step(&p, PLAYER_INPUT_UP, &vine, 1, NULL, 0, NULL, 0) == 1);
        CHECK(p.on_vine);
        steps++;
    }
    CHECK(p.y == top_y);
    CHECK(abs(steps - (int)ceilf((start_y - top_y) * 60.0f / CLIMB_SPEED)) <= 1);
    /* Holding Up longer stays at the top. */
    for (int i = 0; i < 30; i++) climb_step(&p, PLAYER_INPUT_UP, &vine, 1, NULL, 0, NULL, 0);
    CHECK(p.on_vine && p.y == top_y);

    steps = 0;
    while (p.on_vine && steps < 600) {
        climb_step(&p, PLAYER_INPUT_DOWN, &vine, 1, NULL, 0, NULL, 0);
        steps++;
    }
    CHECK(!p.on_vine);
    CHECK(p.y + PLAYER_FRAME_H - PLAYER_FLOOR_SINK > bottom);
    CHECK(abs(steps - (int)ceilf((bottom - (top_y + PLAYER_FRAME_H - PLAYER_FLOOR_SINK))
                                 * 60.0f / CLIMB_SPEED)) <= 1);
done:
    return failed;
}
