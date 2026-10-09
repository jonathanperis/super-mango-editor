/*
 * gameplay_mechanics_test.c — Multi-step proofs of climbing, hazards, enemies
 * and bridges, driven through the production update and render paths.
 *
 * Every case loads a small fixture from tests/fixtures/runtime with
 * game_init, holds inputs through gs.replay_input_mask (the same bits a
 * recorded run uses) and advances game_update_active one fixed step at a
 * time. Assertions read the resulting GameState: positions, states, hearts.
 * A few frames also go through game_frame with the debug overlay on, so the
 * entity renderers and the overlay draw every fixture at least once.
 *
 * The scripted-input replay (input/game_replay.c) is covered in
 * game_replay_test.c, linked into this same binary.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collision/collision_damage.h"
#include "core/debug.h"
#include "core/game_experiment.h"
#include "core/game_overlay.h"
#include "core/game_profile.h"
#include "core/game_resume.h"
#include "effects/parallax.h"
#include "render/game_render.h"
#include "screens/settings_menu.h"
#include "core/game_timing.h"
#include "core/game_update.h"
#include "hazards/axe_trap.h"
#include "hazards/blue_flame.h"
#include "hazards/spike.h"
#include "hazards/spike_block.h"
#include "input/game_input.h"
#include "input/input_backend.h"
#include "player/player_internal.h"
#include "shared/audio.h"
#include "gameplay_mechanics_test.h"

#define CLIMBING_LEVEL  "tests/fixtures/runtime/climbing.toml"
#define HAZARDS_LEVEL   "tests/fixtures/runtime/hazards.toml"
#define CREATURES_LEVEL "tests/fixtures/runtime/creatures.toml"
#define FAR_START_LEVEL "tests/fixtures/runtime/far_start.toml"
#define TRANSITION_LEVEL "tests/fixtures/runtime/transition.toml"

#define STEPS_PER_SECOND 60
#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "gameplay_mechanics_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)
#define NEAR(a, b, tolerance) (fabsf((float)(a) - (float)(b)) <= (tolerance))

int mechanics_open_level(GameState *gs, const char *path, int debug)
{
    memset(gs, 0, sizeof(*gs));
    gs->random_seed = 7;
    gs->debug_mode = debug;
    snprintf(gs->level_path, sizeof(gs->level_path), "%s", path);
    if (game_init(gs) != 0) return -1;
    input_clear();
    game_input_test_set_physical_state(0, 0);
    return 0;
}

/* Advance n fixed steps holding the given PLAYER_INPUT_* bits. */
void mechanics_step(GameState *gs, unsigned int input, int n)
{
    for (int i = 0; i < n; i++) {
        gs->replay_input_mask = input;
        game_update_active(gs, GAME_FIXED_STEP, (int)gs->camera.x);
    }
    gs->replay_input_mask = 0;
}

/* Run whole frames (input, fixed step, render) at the deterministic smoke
 * frame length, as `--smoke-test-frames` does. */
int mechanics_frames(GameState *gs, int n)
{
    int presented = 0;
    gs->smoke_test_frames = n + 1;  /* never reaches the smoke exit */
    for (int i = 0; i < n; i++) presented += game_frame(gs);
    gs->smoke_test_frames = 0;
    return presented;
}

static void stand_at(GameState *gs, float x)
{
    Player *p = &gs->player;
    p->x = x;
    p->y = (float)(FLOOR_Y - p->h + PLAYER_FLOOR_SINK);
    p->vx = p->vy = 0.0f;
    p->on_ground = 1;
    p->on_vine = 0;
    p->jump_held = 0;
    p->hurt_timer = 0.0f;
    gs->loop.fp_prev_riding = -1;
}

/*
 * Put the player's hitbox centre on the centre of target, in mid-air, and
 * run one step. Returns 1 when that step cost a heart.
 */
static int touch_costs_heart(GameState *gs, IntRect target)
{
    Player *p = &gs->player;
    IntRect body = player_get_hitbox(p);
    p->x = target.x + target.w / 2.0f - body.w / 2.0f - PLAYER_PHYS_PAD_X;
    p->y = target.y + target.h / 2.0f - body.h / 2.0f - PLAYER_PHYS_PAD_TOP;
    p->vx = p->vy = 0.0f;
    p->on_ground = p->on_vine = 0;
    p->hurt_timer = 0.0f;
    gs->hearts = 3;
    int hearts = gs->hearts;
    mechanics_step(gs, 0, 1);
    return gs->hearts == hearts - 1 && p->hurt_timer > 0.0f;
}

/* ------------------------------------------------------------------ */
/* Climbing                                                            */
/* ------------------------------------------------------------------ */

static int climbing_ladder_rope_and_vine(void)
{
    int failed = 0;
    GameState gs;
    Player *p = &gs.player;
    CHECK(mechanics_open_level(&gs, CLIMBING_LEVEL, 1) == 0);
    CHECK(gs.ladder_count == 1 && gs.rope_count == 1 && gs.vine_count == 1);
    const float floor_y = (float)(FLOOR_Y - p->h + PLAYER_FLOOR_SINK);
    const float ladder_top = gs.ladders[0].y;

    /* Up with nothing to hold does nothing: the spawn is far from all three. */
    mechanics_step(&gs, PLAYER_INPUT_UP, 5);
    CHECK(!p->on_vine && p->on_ground && NEAR(p->y, floor_y, 0.01f));

    /* Up at the ladder grabs it: no gravity, 80 px/s upwards. */
    stand_at(&gs, gs.ladders[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 1);
    CHECK(p->on_vine && p->climb_source == 1 && p->vine_index == 0);
    CHECK(!p->on_ground && p->vy < 0.0f);
    float before = p->y;
    mechanics_step(&gs, PLAYER_INPUT_UP, 30);
    CHECK(NEAR(before - p->y, 40.0f, 0.5f));
    CHECK(p->anim_state == ANIM_CLIMB);

    /* Holding Up past the top clamps the head at the ladder top. */
    mechanics_step(&gs, PLAYER_INPUT_UP, 3 * STEPS_PER_SECOND);
    CHECK(p->on_vine && NEAR(p->y + PLAYER_PHYS_PAD_TOP, ladder_top, 0.01f));

    /* No input while climbing: the player hangs still. */
    before = p->y;
    mechanics_step(&gs, 0, 30);
    CHECK(p->on_vine && NEAR(p->y, before, 0.001f));

    /* Climbing down past the bottom lets go, and the floor catches the
     * player instead of letting them sink through it. */
    mechanics_step(&gs, PLAYER_INPUT_DOWN, 3 * STEPS_PER_SECOND);
    CHECK(!p->on_vine);
    mechanics_step(&gs, 0, 10);
    CHECK(p->on_ground && NEAR(p->y, floor_y, 0.01f));

    /* Drifting sideways off the grab zone also lets go, then gravity wins. */
    stand_at(&gs, gs.ladders[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 40);
    CHECK(p->on_vine);
    int steps_to_detach = 0;
    while (p->on_vine && steps_to_detach < STEPS_PER_SECOND) {
        mechanics_step(&gs, PLAYER_INPUT_RIGHT, 1);
        steps_to_detach++;
    }
    CHECK(!p->on_vine && steps_to_detach < 20 && !p->facing_left);
    mechanics_step(&gs, 0, 2 * STEPS_PER_SECOND);
    CHECK(p->on_ground && NEAR(p->y, floor_y, 0.01f));

    /* Jump releases the ladder with a full jump impulse. */
    stand_at(&gs, gs.ladders[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 40);
    CHECK(p->on_vine);
    mechanics_step(&gs, PLAYER_INPUT_JUMP, 1);
    CHECK(!p->on_vine && p->vy < JUMP_VY + GRAVITY * GAME_FIXED_STEP + 0.01f);
    mechanics_step(&gs, 0, 2 * STEPS_PER_SECOND);
    CHECK(p->on_ground);

    /* Up + Jump together never grabs: it would grab and jump off again on
     * every step, gaining height each time. */
    stand_at(&gs, gs.ladders[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP | PLAYER_INPUT_JUMP, 1);
    CHECK(!p->on_vine && p->vy < 0.0f);
    mechanics_step(&gs, 0, 2 * STEPS_PER_SECOND);

    /* The rope and the vine use the same rules with their own heights. */
    stand_at(&gs, gs.ropes[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 1);
    CHECK(p->on_vine && p->climb_source == 2);
    mechanics_step(&gs, PLAYER_INPUT_UP | PLAYER_INPUT_LEFT, 3 * STEPS_PER_SECOND);
    CHECK(!p->on_vine && p->facing_left);  /* left drifts off the rope */
    mechanics_step(&gs, 0, 2 * STEPS_PER_SECOND);

    stand_at(&gs, gs.vines[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 1);
    CHECK(p->on_vine && p->climb_source == 0);
    mechanics_step(&gs, PLAYER_INPUT_UP, 3 * STEPS_PER_SECOND);
    CHECK(p->on_vine && NEAR(p->y + PLAYER_PHYS_PAD_TOP, gs.vines[0].y, 0.01f));

    /* Draw the climbing state with the debug overlay on. */
    CHECK(mechanics_frames(&gs, 3) == 3);
    CHECK(p->on_vine);
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * A hit while climbing throws the player off with the usual knockback, even
 * while Up stays held. The climbing controls used to overwrite the push on
 * the next step, and holding Up would re-grab the ladder at once.
 */
static int knockback_throws_a_climber_off(void)
{
    int failed = 0;
    GameState gs;
    Player *p = &gs.player;
    CHECK(mechanics_open_level(&gs, CLIMBING_LEVEL, 0) == 0);
    stand_at(&gs, gs.ladders[0].x - 8.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 20);
    mechanics_step(&gs, 0, 1);              /* hang still on the ladder */
    CHECK(p->on_vine && p->vx == 0.0f && p->vy == 0.0f);

    /* A hit from the right of a still player pushes it left and up. */
    const float x_before = p->x;
    apply_damage(&gs, 1, 1, p->x + p->w, p->y);
    CHECK(!p->on_vine && p->vx < 0.0f && p->vy < 0.0f);
    mechanics_step(&gs, PLAYER_INPUT_UP, 1);
    CHECK(!p->on_vine && p->x < x_before - 3.0f && p->vy < 0.0f);

    /* Still holding Up, the player flies clear instead of re-grabbing. */
    mechanics_step(&gs, PLAYER_INPUT_UP, STEPS_PER_SECOND / 2);
    CHECK(!p->on_vine && p->x < x_before - 40.0f);
done:
    game_cleanup(&gs);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Hazards                                                             */
/* ------------------------------------------------------------------ */

static int blue_flame_erupts_on_a_cycle_and_burns(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, HAZARDS_LEVEL, 0) == 0);
    CHECK(gs.blue_flame_count == 1);
    BlueFlame *flame = &gs.blue_flames[0];
    const float start_y = flame->start_y;
    CHECK(flame->state == BLUE_FLAME_WAITING && NEAR(flame->y, start_y, 0.01f));

    /* Record the step on which each state begins, over one and a bit
     * cycles. The first flame's wait timer starts at zero. */
    int began[4][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
    BlueFlameState last = flame->state;
    float apex_y = start_y, apex_angle = 0.0f;
    for (int step = 1; step <= 5 * STEPS_PER_SECOND; step++) {
        mechanics_step(&gs, 0, 1);
        if (flame->y < apex_y) apex_y = flame->y;
        if (flame->state == BLUE_FLAME_FLIPPING) apex_angle = flame->angle;
        if (flame->state != last) {
            int slot = began[flame->state][0] < 0 ? 0 : 1;
            if (began[flame->state][slot] < 0) began[flame->state][slot] = step;
            last = flame->state;
        }
    }
    /* Launch at 550 px/s against 800 px/s² of drag: 0.6875 s up. */
    CHECK(began[BLUE_FLAME_RISING][0] == 1);
    CHECK(abs(began[BLUE_FLAME_FLIPPING][0] - began[BLUE_FLAME_RISING][0] - 41) <= 1);
    /* The 180-degree flip takes 0.12 s. */
    CHECK(abs(began[BLUE_FLAME_FALLING][0] - began[BLUE_FLAME_FLIPPING][0] - 7) <= 1);
    CHECK(apex_angle > 150.0f && apex_angle <= 180.0f);
    /* It falls back as far as it rose, then hides for 1.5 s. */
    CHECK(abs(began[BLUE_FLAME_WAITING][0] - began[BLUE_FLAME_FALLING][0] - 41) <= 2);
    CHECK(abs(began[BLUE_FLAME_RISING][1] - began[BLUE_FLAME_WAITING][0] - 90) <= 1);
    /* v²/2a = 189 px of rise, well above the tallest pillar. */
    CHECK(NEAR(start_y - apex_y, 550.0f * 550.0f / (2.0f * 800.0f), 6.0f));
    CHECK(apex_y < BLUE_FLAME_APEX_Y + 60.0f);

    /* Burns at the apex... */
    for (int n = 0; flame->state != BLUE_FLAME_FLIPPING && n < 10 * STEPS_PER_SECOND; n++)
        mechanics_step(&gs, 0, 1);
    CHECK(flame->state == BLUE_FLAME_FLIPPING);
    CHECK(touch_costs_heart(&gs, blue_flame_get_hitbox(flame)));
    /* ...but a hidden flame under the floor does not. */
    for (int n = 0; flame->state != BLUE_FLAME_WAITING && n < 10 * STEPS_PER_SECOND; n++)
        mechanics_step(&gs, 0, 1);
    CHECK(flame->state == BLUE_FLAME_WAITING);
    CHECK(!touch_costs_heart(&gs, blue_flame_get_hitbox(flame)));

    /* Draw the rising flame with the overlay. */
    gs.debug_mode = 1;
    debug_init(&gs.debug);
    for (int n = 0; flame->state != BLUE_FLAME_RISING && n < 10 * STEPS_PER_SECOND; n++)
        mechanics_step(&gs, 0, 1);
    CHECK(flame->state == BLUE_FLAME_RISING);
    stand_at(&gs, 250.0f);
    CHECK(mechanics_frames(&gs, 4) == 4);
done:
    game_cleanup(&gs);
    return failed;
}

static int axe_traps_swing_spin_and_hit(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, HAZARDS_LEVEL, 0) == 0);
    CHECK(gs.axe_trap_count == 2);
    AxeTrap *pendulum = &gs.axe_traps[0], *spin = &gs.axe_traps[1];
    CHECK(pendulum->mode == AXE_MODE_PENDULUM && spin->mode == AXE_MODE_SPIN);
    /* The pivot is the centre of the authored pillar. */
    CHECK(NEAR(pendulum->x, 560.0f + TILE_SIZE / 2.0f, 0.01f));

    /* A 2 s pendulum: +60 degrees at 0.5 s, back through 0 at 1 s, -60 at
     * 1.5 s. The spinning axe turns 180 degrees per second. */
    mechanics_step(&gs, 0, STEPS_PER_SECOND / 2);
    CHECK(NEAR(pendulum->angle, AXE_SWING_AMPLITUDE, 0.1f));
    CHECK(NEAR(spin->angle, 90.0f, 0.1f));
    mechanics_step(&gs, 0, STEPS_PER_SECOND / 2);
    CHECK(NEAR(pendulum->angle, 0.0f, 0.5f));
    mechanics_step(&gs, 0, STEPS_PER_SECOND / 2);
    CHECK(NEAR(pendulum->angle, -AXE_SWING_AMPLITUDE, 0.1f));
    CHECK(NEAR(spin->angle, 270.0f, 0.1f));
    /* The spin wraps at a full turn instead of growing without bound. */
    mechanics_step(&gs, 0, STEPS_PER_SECOND);
    CHECK(spin->angle >= 0.0f && spin->angle < 360.0f && NEAR(spin->angle, 90.0f, 0.2f));
    /* So does the pendulum's clock, 2.5 s in: it is 0.5 s into its second
     * cycle, and the swing went on through the wrap to +60 degrees again. */
    CHECK(pendulum->time >= 0.0f && pendulum->time < AXE_SWING_PERIOD);
    CHECK(NEAR(pendulum->time, 0.5f, 0.01f));
    CHECK(NEAR(pendulum->angle, AXE_SWING_AMPLITUDE, 0.1f));

    /* The blade's hitbox follows the swing: it moves sideways between the
     * two extremes, and touching it costs a heart. */
    IntRect left_hit = axe_trap_get_hitbox(pendulum);
    mechanics_step(&gs, 0, STEPS_PER_SECOND);
    IntRect right_hit = axe_trap_get_hitbox(pendulum);
    CHECK(abs(left_hit.x - right_hit.x) > 60);
    CHECK(touch_costs_heart(&gs, axe_trap_get_hitbox(pendulum)));
    CHECK(touch_costs_heart(&gs, axe_trap_get_hitbox(spin)));

    /* Draw both axes with the overlay. */
    gs.debug_mode = 1;
    debug_init(&gs.debug);
    stand_at(&gs, 650.0f);
    CHECK(mechanics_frames(&gs, 3) == 3);
done:
    game_cleanup(&gs);
    return failed;
}

static int spikes_hurt_once_per_invincibility(void)
{
    int failed = 0;
    GameState gs;
    Player *p = &gs.player;
    CHECK(mechanics_open_level(&gs, HAZARDS_LEVEL, 0) == 0);
    CHECK(gs.spike_row_count == 1 && gs.spike_rows[0].count == 3);
    IntRect row = spike_row_get_rect(&gs.spike_rows[0]);
    CHECK(row.w == 3 * SPIKE_TILE_W && row.y + row.h == FLOOR_Y);

    /* Walk right into the row: one heart, knocked back to the left. */
    stand_at(&gs, row.x - 60.0f);
    int hearts = gs.hearts, steps = 0;
    while (gs.hearts == hearts && steps < 2 * STEPS_PER_SECOND) {
        mechanics_step(&gs, PLAYER_INPUT_RIGHT, 1);
        steps++;
    }
    CHECK(gs.hearts == hearts - 1 && p->hurt_timer > 0.0f && p->vx < 0.0f);
    IntRect body = player_get_hitbox(p);
    CHECK(spike_row_hit_test(&gs.spike_rows[0], &body) || body.x + body.w >= row.x - 4);

    /* Standing in the spikes during the blink costs nothing more. */
    stand_at(&gs, row.x);
    p->hurt_timer = 1.0f;
    mechanics_step(&gs, 0, 20);
    CHECK(gs.hearts == hearts - 1);
    /* Once it runs out, the spikes bite again. */
    p->hurt_timer = 0.0f;
    mechanics_step(&gs, 0, 1);
    CHECK(gs.hearts == hearts - 2);

    gs.debug_mode = 1;
    debug_init(&gs.debug);
    stand_at(&gs, row.x - 100.0f);
    CHECK(mechanics_frames(&gs, 2) == 2);
done:
    game_cleanup(&gs);
    return failed;
}

/* Centre of a spike block, for comparisons along its rail. */
static void block_centre(const SpikeBlock *block, float *x, float *y)
{
    *x = block->x + block->w * 0.5f;
    *y = block->y + block->h * 0.5f;
}

static int spike_blocks_follow_their_rails(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, HAZARDS_LEVEL, 0) == 0);
    CHECK(gs.spike_block_count == 3 && gs.rail_count == 3);
    SpikeBlock *loop = &gs.spike_blocks[0];
    SpikeBlock *open = &gs.spike_blocks[1];
    SpikeBlock *capped = &gs.spike_blocks[2];
    CHECK(gs.rails[0].closed && !gs.rails[1].closed && !gs.rails[1].end_cap);
    CHECK(!gs.rails[2].closed && gs.rails[2].end_cap);

    /* Closed loop: the block circles at 3 tiles/s, never leaves the
     * rail's rectangle, and is back where it started after one lap. */
    float x0, y0, x, y;
    block_centre(loop, &x0, &y0);
    const float lap_seconds = (float)gs.rails[0].count / loop->speed;
    const int lap_steps = (int)lroundf(lap_seconds * STEPS_PER_SECOND);
    float min_x = x0, max_x = x0, min_y = y0, max_y = y0;
    for (int i = 0; i < lap_steps; i++) {
        mechanics_step(&gs, 0, 1);
        block_centre(loop, &x, &y);
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    block_centre(loop, &x, &y);
    CHECK(NEAR(x, x0, 1.0f) && NEAR(y, y0, 1.0f));
    CHECK(min_x >= 1200.0f && max_x <= 1200.0f + 6 * RAIL_TILE_W);
    CHECK(min_y >= 40.0f && max_y <= 40.0f + 4 * RAIL_TILE_H);
    CHECK(max_x - min_x > 3 * RAIL_TILE_W && max_y - min_y > RAIL_TILE_H);

    /* An open rail's block waits off-screen, so the player never meets a
     * block that already fell off its rail. */
    CHECK(open->waiting && open->active && open->t == 0.0f);

    /* Bring both open rails into view by moving the player there. */
    stand_at(&gs, 1650.0f);
    gs.camera.x = 1600.0f;
    mechanics_step(&gs, 0, 1);
    CHECK(!open->waiting && open->t > 0.0f);

    /* With no end cap the block runs off the end, keeps its speed while it
     * falls, and is removed once it is below the screen. */
    int detached_step = -1, removed_step = -1;
    float detach_x = 0.0f;
    for (int step = 0; step < 5 * STEPS_PER_SECOND && removed_step < 0; step++) {
        mechanics_step(&gs, 0, 1);
        if (open->detached && detached_step < 0) {
            detached_step = step;
            detach_x = open->x;
        }
        if (!open->active) removed_step = step;
    }
    /* 7 tiles at 4 tiles/s is 1.75 s of riding. */
    CHECK(detached_step >= 0 && abs(detached_step - 105) <= 2);
    CHECK(removed_step > detached_step);
    CHECK(open->x > detach_x && open->y > GAME_H);

    /* The capped rail turns its block around at each end instead. */
    int saw_reverse = 0, saw_forward_again = 0;
    for (int step = 0; step < 5 * STEPS_PER_SECOND; step++) {
        mechanics_step(&gs, 0, 1);
        if (capped->direction < 0) saw_reverse = 1;
        if (saw_reverse && capped->direction > 0) saw_forward_again = 1;
        CHECK(capped->active && !capped->detached);
        CHECK(capped->t >= 0.0f && capped->t <= (float)(gs.rails[2].count - 1));
    }
    CHECK(saw_reverse && saw_forward_again);

    /* Riding blocks hurt. */
    CHECK(touch_costs_heart(&gs, spike_block_get_hitbox(capped)));

    gs.debug_mode = 1;
    debug_init(&gs.debug);
    stand_at(&gs, 1250.0f);
    CHECK(mechanics_frames(&gs, 3) == 3);
done:
    game_cleanup(&gs);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Enemies and bridges                                                 */
/* ------------------------------------------------------------------ */

static int jumping_spider_leaps_the_gap(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    CHECK(gs.jumping_spider_count == 1 && gs.floor_gap_count == 2);
    JumpingSpider *spider = &gs.jumping_spiders[0];
    const float gap = (float)gs.floor_gaps[0];
    /* Keep the player well away on the far side of the level. */
    stand_at(&gs, 1000.0f);

    float min_y = 0.0f;
    int took_off = -1, landed = -1;
    for (int step = 0; step < 3 * STEPS_PER_SECOND && landed < 0; step++) {
        mechanics_step(&gs, 0, 1);
        if (!spider->on_ground && took_off < 0) took_off = step;
        if (spider->y < min_y) min_y = spider->y;
        if (took_off >= 0 && spider->on_ground) landed = step;
        /* Never walks into the hole: grounded means the body is beside it. */
        float centre = spider->x + JSPIDER_ART_X + JSPIDER_ART_W / 2.0f;
        if (spider->on_ground)
            CHECK(centre < gap + 0.01f || centre >= gap + FLOOR_GAP_W - 0.01f);
    }
    CHECK(took_off >= 0 && landed > took_off);
    /* 200 px/s up against 600 px/s²: about 33 px high, 0.67 s in the air. */
    CHECK(NEAR(-min_y, 200.0f * 200.0f / (2.0f * 600.0f), 2.0f));
    CHECK(abs((landed - took_off) - 40) <= 2);
    CHECK(spider->y == 0.0f && spider->vx > 0.0f);
    CHECK(spider->x + JSPIDER_ART_X >= gap + FLOOR_GAP_W - JSPIDER_ART_W);

    /* It turns at the end of its patrol and jumps the gap again going home. */
    int turned = 0, jumped_back = 0;
    for (int step = 0; step < 8 * STEPS_PER_SECOND && !jumped_back; step++) {
        mechanics_step(&gs, 0, 1);
        if (spider->vx < 0.0f) turned = 1;
        if (turned && !spider->on_ground) jumped_back = 1;
        CHECK(spider->x >= 48.0f - 0.01f && spider->x + JSPIDER_FRAME_W <= 320.0f + 0.01f);
    }
    CHECK(turned && jumped_back);
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * The fixture spider is authored at -150 px/s, three times SPIDER_SPEED. It
 * must keep that speed after turning at the floor gap and at its patrol end;
 * the old code dropped it to SPIDER_SPEED at the first turn.
 */
static int spider_keeps_its_authored_speed(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    CHECK(gs.spider_count == 1);
    Spider *spider = &gs.spiders[0];
    const float gap = (float)gs.floor_gaps[0];
    CHECK(spider->vx == -150.0f);
    stand_at(&gs, 1000.0f);

    int gap_turn = 0, end_turn = 0;
    for (int step = 0; step < 4 * STEPS_PER_SECOND && !end_turn; step++) {
        float before = spider->vx;
        mechanics_step(&gs, 0, 1);
        if (before < 0.0f && spider->vx > 0.0f) gap_turn = 1;
        if (gap_turn && before > 0.0f && spider->vx < 0.0f) end_turn = 1;
        CHECK(fabsf(spider->vx) == 150.0f);
        /* Never over the hole: the gap check still catches it every step. */
        float centre = spider->x + SPIDER_ART_X + SPIDER_ART_W / 2.0f;
        CHECK(centre < gap || centre >= gap + FLOOR_GAP_W);
    }
    CHECK(gap_turn && end_turn);
    CHECK(NEAR(spider->x + SPIDER_FRAME_W, 400.0f, 0.01f));
    /* A full step later it has walked 150 / 60 = 2.5 px back. */
    mechanics_step(&gs, 0, 1);
    CHECK(NEAR(spider->x + SPIDER_FRAME_W, 397.5f, 0.01f));
done:
    game_cleanup(&gs);
    return failed;
}

static int fish_leap_from_the_water_and_patrol(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    CHECK(gs.fish_count == 1);
    Fish *fish = &gs.fish[0];
    const float water_y = fish->water_y;
    CHECK(fish->y == water_y && fish->vx > 0.0f);
    stand_at(&gs, 1000.0f);

    /* Jump delays are 1.4-3.0 s; give it time for at least one leap. */
    float top = water_y;
    int leaps = 0, turned = 0;
    for (int step = 0; step < 7 * STEPS_PER_SECOND; step++) {
        float before = fish->vy;
        mechanics_step(&gs, 0, 1);
        if (before == 0.0f && fish->vy < 0.0f) leaps++;
        if (fish->y < top) top = fish->y;
        if (fish->vx < 0.0f) turned = 1;
        CHECK(fish->y <= water_y);
        CHECK(fish->x >= 560.0f - 0.01f && fish->x + FISH_RENDER_W <= 860.0f + 0.01f);
    }
    CHECK(leaps >= 2);
    /* 280 px/s up under 800 px/s²: about 49 px above the water. */
    CHECK(NEAR(water_y - top, 280.0f * 280.0f / (2.0f * 800.0f), 3.0f));
    CHECK(turned);

    /* The fish is dangerous mid-leap. */
    for (int n = 0; !(fish->vy < 0.0f) && n < 10 * STEPS_PER_SECOND; n++)
        mechanics_step(&gs, 0, 1);
    CHECK(fish->vy < 0.0f);
    CHECK(touch_costs_heart(&gs, fish_get_hitbox(fish)));
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * The faster fish runs the shared fish code with its own tuning: it patrols
 * at its authored 120 px/s, leaps -420 px/s every 1.0-2.2 s (about 110 px
 * above the water) and hurts on contact.
 */
static int faster_fish_leap_higher_and_patrol_faster(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    CHECK(gs.faster_fish_count == 1);
    FasterFish *fish = &gs.faster_fish[0];
    const float water_y = fish->water_y;
    CHECK(fish->y == water_y && fish->vx == -120.0f);
    stand_at(&gs, 1000.0f);

    /* Ten steps at 120 px/s: 20 px to the left, before any turn. */
    const float start_x = fish->x;
    mechanics_step(&gs, 0, 10);
    CHECK(NEAR(start_x - fish->x, 20.0f, 0.01f));

    float top = water_y;
    int leaps = 0, turned_right = 0, turned_left = 0;
    for (int step = 0; step < 6 * STEPS_PER_SECOND; step++) {
        float before_vx = fish->vx, before_vy = fish->vy;
        mechanics_step(&gs, 0, 1);
        if (before_vy == 0.0f && fish->vy < 0.0f) leaps++;
        if (before_vx < 0.0f && fish->vx > 0.0f) turned_right = 1;
        if (before_vx > 0.0f && fish->vx < 0.0f) turned_left = 1;
        if (fish->y < top) top = fish->y;
        CHECK(fabsf(fish->vx) == 120.0f);
        CHECK(fish->y <= water_y);
        CHECK(fish->x >= 400.0f - 0.01f && fish->x + FISH_RENDER_W <= 560.0f + 0.01f);
    }
    CHECK(leaps >= 2 && turned_right && turned_left);
    /* 420 px/s up under 800 px/s²: about 110 px above the water. */
    CHECK(NEAR(water_y - top, 420.0f * 420.0f / (2.0f * 800.0f), 4.0f));

    /* Mid-leap it costs a heart, like the regular fish. */
    for (int n = 0; !(fish->vy < 0.0f) && n < 5 * STEPS_PER_SECOND; n++)
        mechanics_step(&gs, 0, 1);
    CHECK(fish->vy < 0.0f);
    CHECK(touch_costs_heart(&gs, faster_fish_get_hitbox(fish)));
done:
    game_cleanup(&gs);
    return failed;
}

static int birds_patrol_at_their_own_speeds(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    CHECK(gs.bird_count == 1 && gs.faster_bird_count == 1);
    Bird *bird = &gs.birds[0];
    const float bird_x = bird->x, fast_x = gs.faster_birds[0].x;
    stand_at(&gs, 1000.0f);

    /* One second: the slow bird covers 45 px, the faster one 80 px. */
    mechanics_step(&gs, 0, STEPS_PER_SECOND);
    CHECK(NEAR(bird->x - bird_x, 45.0f, 0.5f));
    CHECK(NEAR(fast_x - gs.faster_birds[0].x, 80.0f, 0.5f));

    /* The hitbox bobs on a sine wave as the bird flies. */
    int min_y = 1000, max_y = -1000;
    for (int step = 0; step < 4 * STEPS_PER_SECOND; step++) {
        mechanics_step(&gs, 0, 1);
        IntRect hit = bird_get_hitbox(bird);
        if (hit.y < min_y) min_y = hit.y;
        if (hit.y > max_y) max_y = hit.y;
    }
    CHECK(max_y - min_y > 10 && max_y - min_y <= 41);

    /* Both turn at the ends of their patrols and stay inside them. */
    int bird_turned = 0, fast_turned = 0;
    for (int step = 0; step < 10 * STEPS_PER_SECOND; step++) {
        mechanics_step(&gs, 0, 1);
        if (bird->vx < 0.0f) bird_turned = 1;
        if (gs.faster_birds[0].vx > 0.0f) fast_turned = 1;
        CHECK(bird->x >= -0.01f && bird->x + BIRD_FRAME_W <= 400.0f + 0.01f);
    }
    CHECK(bird_turned && fast_turned);

    /* Touching a bird costs a heart. */
    CHECK(touch_costs_heart(&gs, bird_get_hitbox(bird)));

    /* Draw every creature with the overlay. */
    gs.debug_mode = 1;
    debug_init(&gs.debug);
    stand_at(&gs, 100.0f);
    CHECK(mechanics_frames(&gs, 3) == 3);
    stand_at(&gs, 650.0f);
    gs.camera.x = 450.0f;
    CHECK(mechanics_frames(&gs, 3) == 3);
done:
    game_cleanup(&gs);
    return failed;
}

static int bridge_crumbles_under_the_player(void)
{
    int failed = 0;
    GameState gs;
    Player *p = &gs.player;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 1) == 0);
    CHECK(gs.bridge_count == 1 && gs.bridges[0].brick_count == 8);
    Bridge *bridge = &gs.bridges[0];
    const float on_bridge_y = bridge->base_y - p->h + PLAYER_FLOOR_SINK;

    /* Stand on brick 2: the bridge holds the player up... */
    stand_at(&gs, bridge->x + 2 * BRIDGE_TILE_W + BRIDGE_TILE_W / 2.0f - p->w / 2.0f);
    p->y = on_bridge_y;
    mechanics_step(&gs, 0, 1);
    CHECK(p->on_ground && NEAR(p->y, on_bridge_y, 0.01f));
    CHECK(bridge->bricks[2].fall_delay >= 0.0f && !bridge->bricks[2].falling);
    /* ...the debug log names the brick that was touched... */
    int logged = 0;
    for (int i = 0; i < gs.debug.log_count; i++)
        logged |= strstr(gs.debug.log[i].text, "BRIDGE brick[2] touched") != NULL;
    CHECK(logged);
    /* ...for 0.2 s, then the brick drops and the player with it. */
    mechanics_step(&gs, 0, 9);
    CHECK(!bridge->bricks[2].falling && p->on_ground);
    mechanics_step(&gs, 0, 3);
    CHECK(bridge->bricks[2].falling && bridge->bricks[2].y_offset > 0.0f);
    CHECK(!bridge->bricks[0].falling && !bridge->bricks[7].falling);
    mechanics_step(&gs, 0, STEPS_PER_SECOND);
    CHECK(p->on_ground && NEAR(p->y, (float)(FLOOR_Y - p->h + PLAYER_FLOOR_SINK), 0.01f));
    /* Untouched bricks stay put; the fallen one is removed below the screen. */
    mechanics_step(&gs, 0, 2 * STEPS_PER_SECOND);
    CHECK(!bridge->bricks[2].active);
    CHECK(bridge->bricks[0].active && !bridge->bricks[0].falling);
    CHECK(bridge->bricks[7].active && !bridge->bricks[7].falling);
    CHECK(bridge_has_solid_at(bridge, bridge->x + 1.0f) &&
          !bridge_has_solid_at(bridge, bridge->x + 2 * BRIDGE_TILE_W + 1.0f));

    /* Running along the remaining bricks crumbles each one behind the
     * player in turn: a cascade from left to right. */
    stand_at(&gs, bridge->x + 3 * BRIDGE_TILE_W);
    p->y = on_bridge_y;
    const int first_brick = (int)((p->x + p->w / 2.0f - bridge->x) / BRIDGE_TILE_W);
    int touched = 0;
    for (int step = 0; step < STEPS_PER_SECOND && p->y <= on_bridge_y + 0.01f; step++)
        mechanics_step(&gs, PLAYER_INPUT_RIGHT, 1);
    for (int i = first_brick; i < bridge->brick_count; i++)
        touched += bridge->bricks[i].fall_delay >= 0.0f || bridge->bricks[i].falling;
    CHECK(touched >= 2);
    CHECK(bridge->bricks[first_brick].falling || !bridge->bricks[first_brick].active);

    stand_at(&gs, bridge->x);
    gs.camera.x = bridge->x - 100.0f;
    CHECK(mechanics_frames(&gs, 2) == 2);
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * A player standing on a platform 2 px below the bridge's top, over a solid
 * brick, stands on the platform, not the bridge: that is what the landing
 * test decided. The bridge update used to guess again from "feet within 4 px
 * of the bridge top" and crumbled the brick anyway.
 */
static int bridge_ignores_a_player_on_another_surface(void)
{
    int failed = 0;
    GameState gs;
    Player *p = &gs.player;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    Bridge *bridge = &gs.bridges[0];
    gs.platform_count = 1;
    gs.platforms[0] = (Platform){.x = bridge->x, .y = bridge->base_y + 2.0f,
                                 .w = bridge->brick_count * BRIDGE_TILE_W, .h = 64};
    stand_at(&gs, bridge->x + 2 * BRIDGE_TILE_W + BRIDGE_TILE_W / 2.0f - p->w / 2.0f);
    p->y = gs.platforms[0].y - p->h + PLAYER_FLOOR_SINK;
    mechanics_step(&gs, 0, STEPS_PER_SECOND / 2);
    CHECK(p->on_ground);
    CHECK(NEAR(p->y + p->h - PLAYER_FLOOR_SINK, gs.platforms[0].y, 0.01f));
    for (int i = 0; i < bridge->brick_count; i++)
        CHECK(bridge->bricks[i].fall_delay < 0.0f && !bridge->bricks[i].falling);
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * After losing a life far from the respawn point, and after Retry, the
 * camera is already on the respawn point; it used to pan back across the
 * level from where the player died.
 */
static int camera_jumps_to_the_respawn_point(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 0) == 0);
    stand_at(&gs, 1200.0f);
    gs.player.y = 0.0f;   /* above every enemy and hazard */
    gs.camera.x = 1000.0f;
    mechanics_step(&gs, 0, 1);
    CHECK(gs.camera.x > 900.0f);

    /* Lose a life with the respawn back at the level start (x = 48, where
     * the camera rests at 0), not at the screen checkpoint just saved. */
    gs.respawn_x = 48.0f;
    gs.respawn_y = 252.0f;
    gs.hearts = 1;
    gs.player.hurt_timer = 0.0f;
    apply_damage(&gs, 1, 0, 0.0f, 0.0f);
    CHECK(NEAR(gs.player.x, 48.0f + (TILE_SIZE - gs.player.w) / 2.0f, 0.01f));
    CHECK(gs.camera.x == 0.0f);

    /* Retry after game over does the same. */
    gs.camera.x = 1000.0f;
    gs.lives = 0;
    gs.hearts = 1;
    apply_damage(&gs, 1, 0, 0.0f, 0.0f);
    CHECK(gs.game_over);
    game_restart_after_game_over(&gs);
    CHECK(!gs.game_over && gs.camera.x == 0.0f);
done:
    game_cleanup(&gs);
    return failed;
}

/* Where the camera rests for the player's current position: centred on
 * the player, clamped to the world (no lookahead: these players stand still). */
static float resting_camera_x(const GameState *gs)
{
    float x = gs->player.x + gs->player.w * 0.5f - GAME_W * 0.5f;
    if (x < 0.0f) x = 0.0f;
    if (x > (float)(gs->runtime.world_w - GAME_W)) x = (float)(gs->runtime.world_w - GAME_W);
    return x;
}

/*
 * A level that starts far from x = 0 shows its start on the very first
 * frame: game_init snaps the camera instead of leaving it at 0 for the
 * easing to pan across. A phase transition and an F8 experiment restart
 * place the player the same way, so they snap too.
 */
static int level_start_shows_the_start_at_once(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, FAR_START_LEVEL, 1) == 0);
    float rest = resting_camera_x(&gs);
    CHECK(rest > 600.0f);
    CHECK(gs.camera.x == rest);
    /* One idle step leaves it there: nothing was left to ease toward. */
    mechanics_step(&gs, 0, 1);
    CHECK(NEAR(gs.camera.x, rest, 0.5f));

    /* F8 restarts at the start: the camera follows at once. */
    gs.camera.x = 0.0f;
    CHECK(game_experiment_begin(&gs) == 0);
    CHECK(gs.camera.x == resting_camera_x(&gs) && gs.camera.x > 600.0f);
    game_cleanup(&gs);

    /* A phase transition puts the camera on the new level's start, not
     * where the old level's camera happened to be. */
    CHECK(mechanics_open_level(&gs, TRANSITION_LEVEL, 0) == 0);
    gs.camera.x = 300.0f;
    game_complete_level(&gs);
    CHECK(game_load_next_phase(&gs) == 0);
    CHECK(gs.camera.x == resting_camera_x(&gs));
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * game_resume_apply accepts only a Continue point this exact level file
 * could have produced, and otherwise changes nothing. The transition
 * fixture has one authored checkpoint at (304, 252) and no coins.
 */
static int continue_point_must_fit_the_level(void)
{
    int failed = 0;
    GameState gs;
    GameResume good, bad;
    CHECK(mechanics_open_level(&gs, TRANSITION_LEVEL, 0) == 0);
    snprintf(gs.profile_level_key, sizeof(gs.profile_level_key), "levels/transition.toml");
    game_resume_capture(&gs, &good);
    good.checkpoint = 0;
    good.respawn_x = 304.0f;
    good.respawn_y = 252.0f;
    good.score = 70;
    good.lives = 1;

#define REJECTS(change) do { bad = good; change; \
        CHECK(game_resume_apply(&gs, &bad) == -1); \
        CHECK(gs.score == 0 && !gs.resumed && gs.checkpoint_index == -1); } while (0)
    REJECTS(snprintf(bad.path, sizeof(bad.path), "levels/other.toml"));
    REJECTS(bad.level_hash ^= 1);          /* the file changed since */
    REJECTS(bad.checkpoint = 1);           /* only checkpoint 0 exists */
    REJECTS(bad.respawn_x = 305.0f);       /* not where checkpoint 0 is */
    REJECTS(bad.checkpoint = -1);          /* no checkpoint: must be the start */
    REJECTS(bad.coins = 1);                /* the fixture places no coins */
    REJECTS(bad.level_score_start = 71);   /* more than the run's score */
#undef REJECTS

    CHECK(game_resume_apply(&gs, &good) == 0);
    CHECK(gs.resumed && gs.score == 70 && gs.lives == 1 && gs.checkpoint_index == 0);
    CHECK(NEAR(gs.player.x, 304.0f + (TILE_SIZE - gs.player.w) / 2.0f, 0.01f));
    CHECK(gs.checkpoint_feedback_kind == CHECKPOINT_FEEDBACK_RESPAWN);
    CHECK(gs.camera.x == resting_camera_x(&gs));
done:
    game_cleanup(&gs);
    return failed;
}

/* ------------------------------------------------------------------ */
/* Debug overlay                                                       */
/* ------------------------------------------------------------------ */

static int debug_log_is_a_bounded_ring(void)
{
    int failed = 0;
    GameState gs;
    CHECK(mechanics_open_level(&gs, CREATURES_LEVEL, 1) == 0);
    DebugOverlay *dbg = &gs.debug;
    debug_init(dbg);
    CHECK(dbg->log_count == 0 && dbg->fps_display == 0);

    /* Ten messages into eight slots: the two oldest are overwritten. */
    for (int i = 0; i < 10; i++) debug_log(dbg, "event %d", i);
    CHECK(dbg->log_count == DEBUG_LOG_MAX_ENTRIES);
    int newest = (dbg->log_head + DEBUG_LOG_MAX_ENTRIES - 1) % DEBUG_LOG_MAX_ENTRIES;
    CHECK(strcmp(dbg->log[newest].text, "event 9") == 0);
    CHECK(strcmp(dbg->log[dbg->log_head].text, "event 2") == 0);
    /* Long messages are cut to the slot, never overflow it. */
    debug_log(dbg, "%0*d", 200, 7);
    newest = (dbg->log_head + DEBUG_LOG_MAX_ENTRIES - 1) % DEBUG_LOG_MAX_ENTRIES;
    CHECK(strlen(dbg->log[newest].text) == DEBUG_LOG_MSG_LEN - 1);

    /* Messages age with game time; the counters refresh twice a second. */
    dbg->fps_prev_ticks -= 2 * DEBUG_FPS_SAMPLE_MS;
    debug_update(dbg, 0.5f);
    CHECK(dbg->fps_frame_count == 0 && dbg->fps_display >= 0);
    CHECK(NEAR(dbg->frame_ms, 500.0f, 0.01f) && NEAR(dbg->frame_ms_display, 500.0f, 0.01f));
    CHECK(dbg->mem_mb >= 0.0f);
    for (int i = 0; i < dbg->log_count; i++) CHECK(NEAR(dbg->log[i].age, 0.5f, 0.001f));

    /* Draw with fresh, fading and expired messages, a hurt player and an
     * authored checkpoint, so every optional panel line appears. */
    debug_log(dbg, "fresh");
    dbg->log[0].age = DEBUG_LOG_DISPLAY_SEC - 0.5f;
    dbg->log[1].age = DEBUG_LOG_DISPLAY_SEC + 1.0f;
    gs.player.hurt_timer = 1.0f;
    gs.checkpoint_index = 0;
    gs.loop.fp_prev_riding = 0;
    BeginDrawing();
    BeginTextureMode(gs.frame_target);
    debug_render(dbg, gs.hud.font, &gs, 0);
    EndTextureMode();
    EndDrawing();
    gs.checkpoint_index = -1;
    gs.loop.fp_prev_riding = -1;
    /* An empty panel draws nothing and takes no height. */
    CHECK(debug_draw_panel(gs.hud.font, 0, 0, 0, NULL, NULL, 0) == 0);
    debug_cleanup(dbg);
done:
    game_cleanup(&gs);
    return failed;
}

/* ------------------------------------------------------------------ */

static int setup_window(void)
{
    if (display_open(GAME_W, GAME_H, "gameplay mechanics", 1)) return 1;
    input_open(GAME_W, GAME_H);
    game_input_test_set_physical_state(0, 0);
    return audio_open() != 0;
}

/* ------------------------------------------------------------------ */
/* Parallax and render smoke                                           */
/* ------------------------------------------------------------------ */

/*
 * Each background layer scrolls at speed × camera and wraps every tex_w
 * pixels, so its tiles always cover the canvas. A missing image leaves its
 * layer empty instead of failing the level.
 */
static int parallax_scrolls_and_wraps(void)
{
    int failed = 0;
    ParallaxSystem ps = {0};
    ParallaxLayer layer = {.tex_w = 384, .speed = 0.5f};
    CHECK(parallax_layer_offset(&layer, 100) == 50);
    CHECK(parallax_layer_offset(&layer, 1000) == 500 % 384);   /* wrapped */
    CHECK(parallax_layer_offset(&layer, 768) == 0);            /* one tile exactly */
    CHECK(parallax_layer_offset(&layer, -10) == 384 - 5);      /* never negative */
    layer.speed = 0.0f;
    CHECK(parallax_layer_offset(&layer, 5000) == 0);           /* static sky */
    layer.tex_w = 0;
    CHECK(parallax_layer_offset(&layer, 100) == 0);            /* failed image */
    /* For every camera, the first tile starts at or left of x = 0 and
     * reaches past it, so no gap opens at the left edge. */
    layer = (ParallaxLayer){.tex_w = 384, .speed = 0.38f};
    for (int cam = 0; cam < 4000; cam += 37) {
        int offset = parallax_layer_offset(&layer, cam);
        CHECK(offset >= 0 && offset < layer.tex_w && -offset + layer.tex_w > 0);
    }

    const char paths[2][64] = {"assets/sprites/backgrounds/sky_blue.png",
                               "assets/sprites/backgrounds/missing_layer.png"};
    const float speeds[2] = {0.0f, 0.25f};
    parallax_init_from_def(&ps, paths, speeds, 2);
    CHECK(ps.count == 2);
    CHECK(ps.layers[0].texture && ps.layers[0].tex_w > 0 && ps.layers[0].speed == 0.0f);
    CHECK(!ps.layers[1].texture && ps.layers[1].tex_w == 0 && ps.layers[1].speed == 0.25f);
done:
    parallax_cleanup(&ps);
    if (!failed && ps.count != 0) failed = 1;
    return failed;
}

/*
 * game_render_frame and the overlays in render_overlay.c must present a
 * frame in every overlay state: plain play, pause, game over, completion
 * with and without a next level, a failed next level, and the settings
 * panel on top. A draw path that failed would leave the frame unpresented
 * or ask for a fatal route.
 */
static int every_overlay_state_renders(void)
{
    int failed = 0;
    GameState gs;
    static GameProfile profile;
    SettingsMenu settings = {0};
    CHECK(mechanics_open_level(&gs, HAZARDS_LEVEL, 1) == 0);
    game_profile_init(&profile);
    gs.profile = &profile;
    gs.settings_menu = &settings;

    CHECK(game_overlay_state(&gs) == GAME_OVERLAY_NONE);
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    game_overlay_toggle_pause(&gs);
    CHECK(game_overlay_state(&gs) == GAME_OVERLAY_PAUSED);
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    game_overlay_resume(&gs);

    gs.game_over = 1;
    CHECK(game_overlay_state(&gs) == GAME_OVERLAY_GAME_OVER);
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    gs.game_over = 0;

    gs.completion.complete = 1;
    gs.completion.pending_next_phase = 1;
    CHECK(game_overlay_state(&gs) == GAME_OVERLAY_LEVEL_COMPLETE);
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    gs.completion.next_phase_failed = 1;
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    gs.completion.pending_next_phase = gs.completion.next_phase_failed = 0;
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);  /* final level */
    gs.completion.complete = 0;

    settings_menu_open(&settings);
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    settings.page = 1;
    CHECK(game_render_frame(&gs, 0, GAME_FIXED_STEP) == 1);
    CHECK(gs.route == GAME_ROUTE_NONE);
done:
    settings_menu_cleanup(&settings);
    gs.settings_menu = NULL;
    gs.profile = NULL;
    game_cleanup(&gs);
    return failed;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const struct { const char *name; int (*run)(void); } cases[] = {
#define CASE(fn) {#fn, fn}
        CASE(climbing_ladder_rope_and_vine),
        CASE(knockback_throws_a_climber_off),
        CASE(blue_flame_erupts_on_a_cycle_and_burns),
        CASE(axe_traps_swing_spin_and_hit),
        CASE(spikes_hurt_once_per_invincibility),
        CASE(spike_blocks_follow_their_rails),
        CASE(jumping_spider_leaps_the_gap),
        CASE(spider_keeps_its_authored_speed),
        CASE(fish_leap_from_the_water_and_patrol),
        CASE(faster_fish_leap_higher_and_patrol_faster),
        CASE(birds_patrol_at_their_own_speeds),
        CASE(bridge_crumbles_under_the_player),
        CASE(bridge_ignores_a_player_on_another_surface),
        CASE(camera_jumps_to_the_respawn_point),
        CASE(level_start_shows_the_start_at_once),
        CASE(continue_point_must_fit_the_level),
        CASE(debug_log_is_a_bounded_ring),
        CASE(parallax_scrolls_and_wraps),
        CASE(every_overlay_state_renders),
        CASE(replay_scripts_drive_the_game),
        CASE(replay_scripts_reject_malformed_files),
#undef CASE
    };
    int failures = 0;
    if (setup_window()) {
        fprintf(stderr, "gameplay_mechanics_test: window/audio setup failed\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int result = cases[i].run();
        printf("mechanics: %s %s\n", cases[i].name, result ? "FAIL" : "PASS");
        failures += result != 0;
        input_clear();
    }
    game_input_test_clear_physical_state();
    input_close();
    audio_close();
    if (IsWindowReady()) CloseWindow();
    printf("gameplay_mechanics_test: %d failing cases\n", failures);
    return failures ? 1 : 0;
}
