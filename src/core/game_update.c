/*
 * game_update.c — One fixed step of the simulation, top to bottom.
 *
 * How to read it:
 *   1. One small update_<things>() function per kind of thing that changes
 *      during a step. Each calls that kind's own module (spiders_update in
 *      spider.c, and so on) with exactly the inputs it needs, so the
 *      function is where you see what a kind depends on: the floor gaps, a
 *      sound, the player's x, the camera.
 *   2. s_update_steps, near the end: a table listing those functions in the
 *      order they run. That order is the simulation order.
 *   3. game_update_active, which moves the player, walks the table, and
 *      moves the camera.
 *
 * The other three tables work the same way: s_level_loaders (level_loader.c)
 * fills the world, s_damage_sources (game_collision.c) lists what hurts the
 * player, and s_render_layers (game_render.c) draws the frame.
 */

#include "game_update.h"
#include "../game.h"  /* GameState: this file reads its fields */

#include "game_bridges.h"
#include "game_camera.h"
#include "game_checkpoint.h"
#include "game_float_platforms.h"
#include "game_player_step.h"
#include "../collision/floor_gap_collision.h"
#include "../collision/game_collision.h"
#include "../effects/fog.h"
#include "../effects/water.h"
#include "../entities/bird.h"
#include "../entities/faster_bird.h"
#include "../entities/faster_fish.h"
#include "../entities/fish.h"
#include "../entities/jumping_spider.h"
#include "../entities/spider.h"
#include "../hazards/axe_trap.h"
#include "../hazards/blue_flame.h"
#include "../hazards/circular_saw.h"
#include "../hazards/spike_block.h"
#include "../levels/level_session.h"  /* game_level_start_point_respawn_y */
#include "../shared/geometry.h"      /* float_same_value */
#include "../surfaces/bouncepad.h"

/*
 * StepInputs — what one step hands to every row, besides the game itself.
 *
 * A row reads the fields its kind needs and ignores the rest.
 */
typedef struct {
    float             dt;      /* seconds this step advances (1/60 in play)   */
    int               cam_x;   /* camera left edge when the step began: some
                                  kinds wait for the camera to reveal them,
                                  or stay silent while off-screen             */
    PlayerStepSupport support; /* the float platform and the bridge the
                                  player landed on in this step (-1 = none)   */
} StepInputs;

/*
 * The player's centre x. Rows that watch the player call this when they
 * run, not once per step: a rail platform may carry the player sideways
 * in between (update_float_platforms).
 */
static float player_centre_x(const GameState *gs)
{
    return gs->world.player.x + gs->world.player.w / 2.0f;
}

/* ------------------------------------------------------------------ */
/* Enemies                                                             */
/* ------------------------------------------------------------------ */

/* Spiders walk their patrol and turn back at floor gaps. */
static void update_spiders(GameState *gs, const StepInputs *in)
{
    spiders_update(gs->world.spiders, gs->world.spider_count, in->dt,
                   gs->world.floor_gaps, gs->world.floor_gap_count);
}

/*
 * Jumping spiders leap the gaps. Their attack sound needs two more inputs,
 * as the birds' flap and the axe's swish do below: the player's x (louder
 * when close) and the camera (silent off screen).
 */
static void update_jumping_spiders(GameState *gs, const StepInputs *in)
{
    jumping_spiders_update(gs->world.jumping_spiders, gs->world.jumping_spider_count, in->dt,
                           gs->world.floor_gaps, gs->world.floor_gap_count,
                           gs->assets.audio.spider_attack, player_centre_x(gs), in->cam_x);
}

static void update_birds(GameState *gs, const StepInputs *in)
{
    birds_update(gs->world.birds, gs->world.bird_count, in->dt, gs->assets.audio.flap,
                 player_centre_x(gs), in->cam_x);
}

static void update_faster_birds(GameState *gs, const StepInputs *in)
{
    faster_birds_update(gs->world.faster_birds, gs->world.faster_bird_count, in->dt,
                        gs->assets.audio.flap, player_centre_x(gs), in->cam_x);
}

/* Fish swim and leap from the water, and stay inside the world's width. */
static void update_fish(GameState *gs, const StepInputs *in)
{
    fish_update(gs->world.fish, gs->world.fish_count, in->dt, gs->world.runtime.world_w,
                &gs->world.random);
}

static void update_faster_fish(GameState *gs, const StepInputs *in)
{
    faster_fish_update(gs->world.faster_fish, gs->world.faster_fish_count, in->dt,
                       gs->world.runtime.world_w, &gs->world.random);
}

/* ------------------------------------------------------------------ */
/* Moving hazards and surfaces                                         */
/* ------------------------------------------------------------------ */

/* Spike blocks wait until the camera reveals them, then ride their rail. */
static void update_spike_blocks(GameState *gs, const StepInputs *in)
{
    spike_blocks_update(gs->world.spike_blocks, gs->world.spike_block_count, in->dt, in->cam_x);
}

/* Float platforms move, and a rail platform carries the player who
 * landed on it (game_float_platforms.c). */
static void update_float_platforms(GameState *gs, const StepInputs *in)
{
    game_float_platforms_update(gs, in->dt, in->support.float_platform);
}

/* Bridge bricks fall; the one under the player starts its timer
 * (game_bridges.c). */
static void update_bridges(GameState *gs, const StepInputs *in)
{
    game_bridges_update(gs, in->dt, in->support.bridge);
}

static void update_axe_traps(GameState *gs, const StepInputs *in)
{
    axe_traps_update(gs->world.axe_traps, gs->world.axe_trap_count, in->dt,
                     gs->assets.audio.axe, player_centre_x(gs), in->cam_x);
}

static void update_circular_saws(GameState *gs, const StepInputs *in)
{
    circular_saws_update(gs->world.circular_saws, gs->world.circular_saw_count, in->dt);
}

/* Blue and fire flames are one module (blue_flame.c) and two arrays. */
static void update_blue_flames(GameState *gs, const StepInputs *in)
{
    blue_flames_update(gs->world.blue_flames, gs->world.blue_flame_count, in->dt);
}

static void update_fire_flames(GameState *gs, const StepInputs *in)
{
    blue_flames_update(gs->world.fire_flames, gs->world.fire_flame_count, in->dt);
}

/* ------------------------------------------------------------------ */
/* Collision and checkpoints                                           */
/* ------------------------------------------------------------------ */

/* Enemy and hazard damage, coins, stars and the level's end
 * (game_collision.c). The only row that can cost a life. */
static void update_collision(GameState *gs, const StepInputs *in)
{
    game_collide(gs, in->dt);
}

/*
 * A level without authored checkpoints saves one at each newly entered
 * screen. That moves respawn_x only; a start-point run also moves
 * respawn_y to the ground there (level_session.c says why).
 */
static void update_screen_checkpoints(GameState *gs, const StepInputs *in)
{
    (void)in;  /* reads the player's position only */
    float respawn_x = gs->world.respawn_x;
    game_checkpoint_update(gs);
    if (!float_same_value(gs->world.respawn_x, respawn_x)) game_level_start_point_respawn_y(gs);
}

/* ------------------------------------------------------------------ */
/* Animation                                                           */
/* ------------------------------------------------------------------ */

/* The level decides whether it has water and fog at all. */
static void update_water(GameState *gs, const StepInputs *in)
{
    if (gs->world.runtime.water_enabled) water_update(&gs->world.water, in->dt);
}

static void update_fog(GameState *gs, const StepInputs *in)
{
    if (gs->world.runtime.fog_enabled) fog_update(&gs->world.fog, in->dt, &gs->world.effects_random);
}

/* One module (bouncepad.c), three arrays: the squash animation a hit
 * started in game_player_step plays out here. */
static void update_bouncepads_medium(GameState *gs, const StepInputs *in)
{
    bouncepads_update(gs->world.bouncepads_medium, gs->world.bouncepad_medium_count, in->dt);
}

static void update_bouncepads_small(GameState *gs, const StepInputs *in)
{
    bouncepads_update(gs->world.bouncepads_small, gs->world.bouncepad_small_count, in->dt);
}

static void update_bouncepads_high(GameState *gs, const StepInputs *in)
{
    bouncepads_update(gs->world.bouncepads_high, gs->world.bouncepad_high_count, in->dt);
}

/* ------------------------------------------------------------------ */
/* The update table                                                    */
/* ------------------------------------------------------------------ */

/*
 * s_update_steps — everything a step does after the player has moved, in
 * the order it happens.
 *
 * Each row is a name and the update_<things>() function above that does
 * the work. `update` is a function pointer: the row stores which function
 * to call, and game_update_active calls it through the pointer. The name
 * is for people: a debugger prints it, and `make docs-drift` checks these
 * names, in this order, against the Update Order table in the manual's
 * architecture page.
 *
 * Order matters:
 *   - Everything that can hurt the player moves before the collision row,
 *     so hitboxes are tested where this step put them.
 *   - Float platforms and bridges read what the player landed on, so they
 *     run after the player (game_update_active moves the player first).
 *   - The last five rows only animate. A step that costs a life skips
 *     them (see game_update_active).
 * Moving a row can change what a seeded run does, and then recorded runs
 * and the replay tests no longer match.
 *
 * To add an enemy, say a bat: write its module (bat.h / bat.c with
 * bats_update and bats_render), then add one row in each table:
 *   - s_level_loaders  (level_loader.c)    load_bats        fills the array
 *   - s_update_steps   (here)              update_bats      moves them
 *   - s_damage_sources (game_collision.c)  bat_touches      hurts the player
 *   - s_render_layers  (game_render.c)     render_bats      draws them
 * Each row needs its small function, written next to the others like it.
 */
typedef void (*UpdateFn)(GameState *gs, const StepInputs *in);

typedef struct {
    const char *name;    /* what the row updates                 */
    UpdateFn    update;  /* the function that does it            */
} UpdateStep;

static const UpdateStep s_update_steps[] = {
    /* name                  update                       */
    /* ---- Enemies ----------------------------------------------- */
    { "spiders",             update_spiders            },
    { "jumping spiders",     update_jumping_spiders    },
    { "birds",               update_birds              },
    { "faster birds",        update_faster_birds       },
    { "fish",                update_fish               },
    { "faster fish",         update_faster_fish        },
    /* ---- Moving hazards and surfaces --------------------------- */
    { "spike blocks",        update_spike_blocks       },
    { "float platforms",     update_float_platforms    },
    { "bridges",             update_bridges            },
    { "axe traps",           update_axe_traps          },
    { "circular saws",       update_circular_saws      },
    { "blue flames",         update_blue_flames        },
    { "fire flames",         update_fire_flames        },
    /* ---- Everything is in place: test it against the player ---- */
    { "collision",           update_collision          },
    { "screen checkpoints",  update_screen_checkpoints },
    /* ---- Animation --------------------------------------------- */
    { "water",               update_water              },
    { "fog",                 update_fog                },
    { "bouncepads (medium)", update_bouncepads_medium  },
    { "bouncepads (small)",  update_bouncepads_small   },
    { "bouncepads (high)",   update_bouncepads_high    },
};

#define UPDATE_STEP_COUNT (sizeof(s_update_steps) / sizeof(s_update_steps[0]))

/*
 * step_is_over — 1 once this step changed the life count or ended the game.
 *
 * A lost life puts the player back at the respawn point and reloads the
 * level's LOAD_EVERY_LIFE rows, and game over freezes the level; either
 * way the rest of the step no longer applies to the world it started in.
 * (A bonus life from a coin ends the step the same way: the count changed.)
 */
static int step_is_over(const GameState *gs, int lives_before)
{
    return gs->screen.game_over || gs->world.lives != lives_before;
}

int game_update_active(GameState *gs, float dt, int cam_x)
{
    const int lives_before = gs->world.lives;
    StepInputs in = { dt, cam_x, { -1, -1 } };

    game_checkpoint_feedback_tick(gs, dt);
    gs->screen.completion.level_elapsed += dt;

    /* The player moves first: input, physics and landing on surfaces. What
     * they landed on goes to the rows that need it. */
    in.support = game_player_step(gs, dt);
    /* Player movement and surface collision resolve before checkpoint sampling. */
    game_checkpoint_update_authored(gs);
    /* Lethal gap damage must run after checkpoint sampling. */
    floor_gap_handle_collision(gs);

    /* Then the rest of the world, one row at a time, until the table ends
     * or the step is over. Today only two things can end it: the fall into
     * a floor gap just above and the collision row. Asking before every row
     * keeps that one rule instead of a special case per row. */
    for (size_t i = 0; i < UPDATE_STEP_COUNT && !step_is_over(gs, lives_before); i++) {
        s_update_steps[i].update(gs, &in);
    }

    /* The camera follows the player even on a step that ended early. */
    return game_camera_update(gs, dt);
}
