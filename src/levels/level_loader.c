/*
 * level_loader.c — Translate LevelDef placement data into live GameState.
 *
 * This is the single file that knows both (a) the layout of every entity struct
 * and (b) the LevelDef placement format.  Entity modules know HOW things behave;
 * level files know WHERE things go; this file is the bridge between the two.
 *
 * level_load  : called once at game_init.
 * level_reset : called on player death via reset_current_level; skips static
 *               geometry that never changes (platforms, rails, sea gaps).
 */

#include <stdlib.h>  /* rand(), RAND_MAX */

#include "level_loader.h"

/* Include every entity header so we can fill their structs directly. */
#include "../player/player.h"
#include "../surfaces/platform.h"
#include "../surfaces/rail.h"
#include "../collectibles/coin.h"
#include "../collectibles/yellow_star.h"
#include "../collectibles/last_star.h"
#include "../entities/spider.h"
#include "../entities/jumping_spider.h"
#include "../entities/bird.h"
#include "../entities/faster_bird.h"
#include "../entities/fish.h"
#include "../entities/faster_fish.h"
#include "../hazards/axe_trap.h"
#include "../hazards/circular_saw.h"
#include "../hazards/spike.h"
#include "../hazards/spike_platform.h"
#include "../hazards/spike_block.h"
#include "../hazards/blue_flame.h"
#include "../surfaces/float_platform.h"
#include "../surfaces/bridge.h"
#include "../surfaces/bouncepad.h"
#include "../surfaces/vine.h"
#include "../surfaces/ladder.h"
#include "../surfaces/rope.h"
#include "../effects/water.h"   /* WATER_ART_H — used for fish water_y computation */

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/*
 * rand_range — return a pseudo-random float in [lo, hi].
 * Used to stagger fish jump timers so they don't all leap simultaneously.
 */
static float rand_range(float lo, float hi) {
    return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

/* ------------------------------------------------------------------ */
/* Static geometry initialisation (only called once at load time)     */
/* ------------------------------------------------------------------ */

/*
 * load_sea_gaps — Copy sea gap x-positions from the level definition.
 *
 * Sea gaps are holes in the ground floor.  Their positions feed into both
 * spider gap-avoidance logic and the blue flame eruption system.
 */
static void load_sea_gaps(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->sea_gap_count; i++)
        gs->sea_gaps[i] = def->sea_gaps[i];
    gs->sea_gap_count = def->sea_gap_count;
}

/*
 * load_rails — Build rail path data from the placement array.
 *
 * Rails must be built before any entity that rides them (spike_blocks,
 * float_platforms in RAIL mode) so their Rail* pointers are valid.
 */
static void load_rails(GameState *gs, const LevelDef *def)
{
    rail_init_from_placements(gs->rails, &gs->rail_count,
                              def->rails, def->rail_count);
}

/*
 * load_platforms — Derive pillar geometry from tile-height placements.
 *
 * Each pillar is shifted 16 px into the floor so the grass top edge meets
 * the ground seamlessly.  Width is always one TILE_SIZE (48 px).
 */
static void load_platforms(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->platform_count; i++) {
        const PlatformPlacement *p = &def->platforms[i];
        gs->platforms[i].x = p->x;
        gs->platforms[i].y = (float)(FLOOR_Y - p->tile_height * TILE_SIZE + 16);
        gs->platforms[i].w = TILE_SIZE;
        gs->platforms[i].h = p->tile_height * TILE_SIZE;
    }
    gs->platform_count = def->platform_count;
}

/* ------------------------------------------------------------------ */
/* Collectibles                                                        */
/* ------------------------------------------------------------------ */

static void load_coins(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->coin_count; i++) {
        gs->coins[i].x      = def->coins[i].x;
        gs->coins[i].y      = def->coins[i].y;
        gs->coins[i].active = 1;
    }
    gs->coin_count = def->coin_count;
}

static void load_yellow_stars(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->yellow_star_count; i++) {
        gs->yellow_stars[i].x      = def->yellow_stars[i].x;
        gs->yellow_stars[i].y      = def->yellow_stars[i].y;
        gs->yellow_stars[i].active = 1;
    }
    gs->yellow_star_count = def->yellow_star_count;
}

static void load_last_star(GameState *gs, const LevelDef *def)
{
    gs->last_star.x         = def->last_star.x;
    gs->last_star.y         = def->last_star.y;
    gs->last_star.w         = LAST_STAR_DISPLAY_W;
    gs->last_star.h         = LAST_STAR_DISPLAY_H;
    gs->last_star.active    = 1;
    gs->last_star.collected = 0;
}

/* ------------------------------------------------------------------ */
/* Enemies                                                             */
/* ------------------------------------------------------------------ */

static void load_spiders(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->spider_count; i++) {
        const SpiderPlacement *p = &def->spiders[i];
        gs->spiders[i].x             = p->x;
        gs->spiders[i].vx            = p->vx;
        gs->spiders[i].patrol_x0     = p->patrol_x0;
        gs->spiders[i].patrol_x1     = p->patrol_x1;
        gs->spiders[i].frame_index   = p->frame_index;
        gs->spiders[i].anim_timer_ms = 0;
    }
    gs->spider_count = def->spider_count;
}

static void load_jumping_spiders(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->jumping_spider_count; i++) {
        const JumpingSpiderPlacement *p = &def->jumping_spiders[i];
        gs->jumping_spiders[i].x             = p->x;
        gs->jumping_spiders[i].y             = 0.0f;  /* always on ground */
        gs->jumping_spiders[i].vx            = p->vx;
        gs->jumping_spiders[i].vy            = 0.0f;
        gs->jumping_spiders[i].patrol_x0     = p->patrol_x0;
        gs->jumping_spiders[i].patrol_x1     = p->patrol_x1;
        gs->jumping_spiders[i].jump_timer    = 0.0f;
        gs->jumping_spiders[i].on_ground     = 1;
        gs->jumping_spiders[i].frame_index   = 0;
        gs->jumping_spiders[i].anim_timer_ms = 0;
    }
    gs->jumping_spider_count = def->jumping_spider_count;
}

static void load_birds(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->bird_count; i++) {
        const BirdPlacement *p = &def->birds[i];
        gs->birds[i].x             = p->x;
        gs->birds[i].base_y        = p->base_y;
        gs->birds[i].vx            = p->vx;
        gs->birds[i].patrol_x0     = p->patrol_x0;
        gs->birds[i].patrol_x1     = p->patrol_x1;
        gs->birds[i].frame_index   = p->frame_index;
        gs->birds[i].anim_timer_ms = 0;
    }
    gs->bird_count = def->bird_count;
}

static void load_faster_birds(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->faster_bird_count; i++) {
        const BirdPlacement *p = &def->faster_birds[i];
        gs->faster_birds[i].x             = p->x;
        gs->faster_birds[i].base_y        = p->base_y;
        gs->faster_birds[i].vx            = p->vx;
        gs->faster_birds[i].patrol_x0     = p->patrol_x0;
        gs->faster_birds[i].patrol_x1     = p->patrol_x1;
        gs->faster_birds[i].frame_index   = p->frame_index;
        gs->faster_birds[i].anim_timer_ms = 0;
    }
    gs->faster_bird_count = def->faster_bird_count;
}

static void load_fish(GameState *gs, const LevelDef *def)
{
    /*
     * water_y — the y-position of the fish while swimming.
     * Fish rest with their sprite half-submerged in the water.
     * WATER_ART_H = 31 px visible water strip; FISH_RENDER_H = 48.
     * water_y = water_surface − (render_h / 2) = (GAME_H − WATER_ART_H) − 24
     */
    float water_y = (float)(GAME_H - WATER_ART_H) - FISH_RENDER_H / 2.0f;

    for (int i = 0; i < def->fish_count; i++) {
        const FishPlacement *p = &def->fish[i];
        gs->fish[i].x             = p->x;
        gs->fish[i].y             = water_y;
        gs->fish[i].vx            = p->vx;
        gs->fish[i].vy            = 0.0f;
        gs->fish[i].patrol_x0     = p->patrol_x0;
        gs->fish[i].patrol_x1     = p->patrol_x1;
        gs->fish[i].jump_timer    = rand_range(FISH_JUMP_MIN, FISH_JUMP_MAX);
        gs->fish[i].water_y       = water_y;
        gs->fish[i].frame_index   = 0;
        gs->fish[i].anim_timer_ms = 0;
    }
    gs->fish_count = def->fish_count;
}

static void load_faster_fish(GameState *gs, const LevelDef *def)
{
    float water_y = (float)(GAME_H - WATER_ART_H) - FFISH_RENDER_H / 2.0f;

    for (int i = 0; i < def->faster_fish_count; i++) {
        const FishPlacement *p = &def->faster_fish[i];
        gs->faster_fish[i].x             = p->x;
        gs->faster_fish[i].y             = water_y;
        gs->faster_fish[i].vx            = p->vx;
        gs->faster_fish[i].vy            = 0.0f;
        gs->faster_fish[i].patrol_x0     = p->patrol_x0;
        gs->faster_fish[i].patrol_x1     = p->patrol_x1;
        gs->faster_fish[i].jump_timer    = rand_range(FFISH_JUMP_MIN, FFISH_JUMP_MAX);
        gs->faster_fish[i].water_y       = water_y;
        gs->faster_fish[i].frame_index   = 0;
        gs->faster_fish[i].anim_timer_ms = 0;
    }
    gs->faster_fish_count = def->faster_fish_count;
}

/* ------------------------------------------------------------------ */
/* Hazards                                                             */
/* ------------------------------------------------------------------ */

static void load_axe_traps(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->axe_trap_count; i++) {
        const AxeTrapPlacement *p = &def->axe_traps[i];
        /*
         * Pivot x: horizontal centre of the host pillar (left_edge + half_width).
         * Pivot y: top surface of a 3-tile pillar.
         */
        gs->axe_traps[i].x            = p->pillar_x + TILE_SIZE / 2.0f;
        gs->axe_traps[i].y            = (float)(FLOOR_Y - 3 * TILE_SIZE + 16);
        gs->axe_traps[i].angle        = 0.0f;
        gs->axe_traps[i].time         = 0.0f;
        gs->axe_traps[i].mode         = p->mode;
        gs->axe_traps[i].sound_played = 0;
        gs->axe_traps[i].active       = 1;
    }
    gs->axe_trap_count = def->axe_trap_count;
}

static void load_circular_saws(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->circular_saw_count; i++) {
        const CircularSawPlacement *p = &def->circular_saws[i];
        /*
         * y is fixed at the 2-tile platform top minus the saw's display height.
         * This puts the saw riding along a flat surface at the height of a
         * 2-tile pillar's top edge.
         */
        gs->circular_saws[i].x          = p->x;
        gs->circular_saws[i].y          = (float)(FLOOR_Y - 2 * TILE_SIZE + 16 - SAW_DISPLAY_H);
        gs->circular_saws[i].w          = SAW_DISPLAY_W;
        gs->circular_saws[i].h          = SAW_DISPLAY_H;
        gs->circular_saws[i].patrol_x0  = p->patrol_x0;
        gs->circular_saws[i].patrol_x1  = p->patrol_x1;
        gs->circular_saws[i].direction  = p->direction;
        gs->circular_saws[i].spin_angle = 0.0f;
        gs->circular_saws[i].active     = 1;
    }
    gs->circular_saw_count = def->circular_saw_count;
}

static void load_spike_rows(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->spike_row_count; i++) {
        const SpikeRowPlacement *p = &def->spike_rows[i];
        /* y: top edge of spikes sits flush with the ground floor surface. */
        gs->spike_rows[i].x      = p->x;
        gs->spike_rows[i].y      = (float)(FLOOR_Y - SPIKE_TILE_H);
        gs->spike_rows[i].count  = p->count;
        gs->spike_rows[i].active = 1;
    }
    gs->spike_row_count = def->spike_row_count;
}

static void load_spike_platforms(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->spike_platform_count; i++) {
        const SpikePlatformPlacement *p = &def->spike_platforms[i];
        gs->spike_platforms[i].x      = p->x;
        gs->spike_platforms[i].y      = p->y;
        gs->spike_platforms[i].w      = p->tile_count * SPIKE_PLAT_PIECE_W;
        gs->spike_platforms[i].active = 1;
    }
    gs->spike_platform_count = def->spike_platform_count;
}

static void load_spike_blocks(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->spike_block_count; i++) {
        const SpikeBlockPlacement *p = &def->spike_blocks[i];
        /*
         * spike_block_init expects a Rail pointer and t_offset.
         * rail_index references the gs->rails array built by load_rails().
         */
        spike_block_init(&gs->spike_blocks[i],
                         &gs->rails[p->rail_index],
                         p->t_offset, p->speed);
    }
    gs->spike_block_count = def->spike_block_count;
}

static void load_blue_flames(GameState *gs)
{
    /*
     * Blue flame positions are derived from the sea_gaps array — each gap
     * that is not at the world edge gets one erupting flame.
     * This function assumes load_sea_gaps has already been called.
     */
    blue_flames_init(gs->blue_flames, &gs->blue_flame_count,
                     gs->sea_gaps, gs->sea_gap_count);
}

/* ------------------------------------------------------------------ */
/* Surfaces                                                            */
/* ------------------------------------------------------------------ */

static void load_float_platforms(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->float_platform_count; i++) {
        const FloatPlatformPlacement *p = &def->float_platforms[i];
        const Rail *rail = (p->mode == FLOAT_PLATFORM_RAIL)
                           ? &gs->rails[p->rail_index]
                           : NULL;
        float stand_lim  = (p->mode == FLOAT_PLATFORM_CRUMBLE)
                           ? CRUMBLE_STAND_LIMIT : 0.0f;

        float_platform_init(&gs->float_platforms[i],
                            p->mode, p->x, p->y, p->tile_count,
                            stand_lim,
                            rail, p->t_offset, p->speed);
    }
    gs->float_platform_count = def->float_platform_count;
}

static void load_bridges(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->bridge_count; i++) {
        const BridgePlacement *p = &def->bridges[i];
        Bridge *b = &gs->bridges[i];

        b->x           = p->x;
        b->base_y      = p->y;
        b->brick_count = p->brick_count;

        /* Reset each brick to its default resting state. */
        for (int k = 0; k < p->brick_count; k++) {
            b->bricks[k].y_offset   = 0.0f;
            b->bricks[k].fall_vy    = 0.0f;
            b->bricks[k].falling    = 0;
            b->bricks[k].active     = 1;
            b->bricks[k].fall_delay = -1.0f;
        }
    }
    gs->bridge_count = def->bridge_count;
}

/*
 * load_bouncepads — Populate the three variant arrays from separate placements.
 *
 * GameState keeps small/medium/high pads in separate arrays because each
 * variant uses its own texture.  LevelDef mirrors this split exactly.
 */
static void load_bouncepads(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->bouncepad_small_count; i++)
        bouncepad_place(&gs->bouncepads_small[i],
                        def->bouncepads_small[i].x,
                        def->bouncepads_small[i].launch_vy,
                        def->bouncepads_small[i].pad_type);
    gs->bouncepad_small_count = def->bouncepad_small_count;

    for (int i = 0; i < def->bouncepad_medium_count; i++)
        bouncepad_place(&gs->bouncepads_medium[i],
                        def->bouncepads_medium[i].x,
                        def->bouncepads_medium[i].launch_vy,
                        def->bouncepads_medium[i].pad_type);
    gs->bouncepad_medium_count = def->bouncepad_medium_count;

    for (int i = 0; i < def->bouncepad_high_count; i++)
        bouncepad_place(&gs->bouncepads_high[i],
                        def->bouncepads_high[i].x,
                        def->bouncepads_high[i].launch_vy,
                        def->bouncepads_high[i].pad_type);
    gs->bouncepad_high_count = def->bouncepad_high_count;
}

/* ------------------------------------------------------------------ */
/* Decorations & climbables                                            */
/* ------------------------------------------------------------------ */

static void load_vines(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->vine_count; i++) {
        gs->vines[i].x          = def->vines[i].x;
        gs->vines[i].y          = def->vines[i].y;
        gs->vines[i].tile_count = def->vines[i].tile_count;
    }
    gs->vine_count = def->vine_count;
}

static void load_ladders(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->ladder_count; i++) {
        gs->ladders[i].x          = def->ladders[i].x;
        gs->ladders[i].y          = def->ladders[i].y;
        gs->ladders[i].tile_count = def->ladders[i].tile_count;
    }
    gs->ladder_count = def->ladder_count;
}

static void load_ropes(GameState *gs, const LevelDef *def)
{
    for (int i = 0; i < def->rope_count; i++) {
        gs->ropes[i].x          = def->ropes[i].x;
        gs->ropes[i].y          = def->ropes[i].y;
        gs->ropes[i].tile_count = def->ropes[i].tile_count;
    }
    gs->rope_count = def->rope_count;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * level_load — Populate all GameState arrays from a LevelDef.
 *
 * Call order matters:
 *   1. Sea gaps (used by blue_flames and spider gap checks).
 *   2. Rails (must exist before spike_blocks / float_platforms reference them).
 *   3. Everything else can follow in any order.
 */
void level_load(GameState *gs, const LevelDef *def)
{
    /* Store a pointer to the active level definition for game.c to read */
    gs->current_level = def;

    /* ---- Static geometry ------------------------------------------ */
    load_sea_gaps(gs, def);
    load_rails(gs, def);
    load_platforms(gs, def);

    /* ---- Collectibles --------------------------------------------- */
    load_coins(gs, def);
    load_yellow_stars(gs, def);
    load_last_star(gs, def);

    /* ---- Enemies -------------------------------------------------- */
    load_spiders(gs, def);
    load_jumping_spiders(gs, def);
    load_birds(gs, def);
    load_faster_birds(gs, def);
    load_fish(gs, def);
    load_faster_fish(gs, def);

    /* ---- Hazards -------------------------------------------------- */
    load_axe_traps(gs, def);
    load_circular_saws(gs, def);
    load_spike_rows(gs, def);
    load_spike_platforms(gs, def);
    load_spike_blocks(gs, def);
    load_blue_flames(gs);   /* derived from sea_gaps; no placement array */

    /* ---- Surfaces ------------------------------------------------- */
    load_float_platforms(gs, def);
    load_bridges(gs, def);
    load_bouncepads(gs, def);

    /* ---- Decorations & climbables --------------------------------- */
    load_vines(gs, def);
    load_ladders(gs, def);
    load_ropes(gs, def);

    /* ---- Level-wide configuration --------------------------------- */

    /*
     * Player spawn — override the defaults set by player_init.
     * player_init has already run by this point, so w/h are valid.
     * FLOOR_SINK is 16 px (defined in player.c); we use the same literal
     * here to keep the spawn formula consistent with player_reset.
     */
    if (def->player_start_x != 0.0f || def->player_start_y != 0.0f) {
        gs->player.spawn_x = def->player_start_x;
        gs->player.spawn_y = def->player_start_y;
        gs->player.x = def->player_start_x + (TILE_SIZE - gs->player.w) / 2.0f;
        gs->player.y = def->player_start_y - gs->player.h + 16;  /* 16 = FLOOR_SINK */
    }

    /* ---- Level-wide configuration ---------------------------------- */
    gs->fog_enabled   = def->fog_enabled;
    gs->water_enabled = def->water_enabled;

    /*
     * Game rules — use level-defined values if set (>0), otherwise fall
     * back to engine defaults defined in hud.h and coin.h.
     */
    gs->hearts          = def->initial_hearts  > 0 ? def->initial_hearts  : MAX_HEARTS;
    gs->lives           = def->initial_lives   > 0 ? def->initial_lives   : DEFAULT_LIVES;
    gs->score           = 0;
    gs->score_per_life  = def->score_per_life  > 0 ? def->score_per_life  : SCORE_PER_LIFE;
    gs->score_life_next = gs->score_per_life;
}

/*
 * level_reset — Reset mutable state after a player death.
 *
 * Skips static geometry (platforms, rails, sea gaps, decorations) because
 * they never change during a play session.  Only enemies, collectibles,
 * hazards, and surfaces need to be reset for a clean restart.
 */
void level_reset(GameState *gs, const LevelDef *def)
{
    player_reset(&gs->player);

    /* Collectibles */
    load_coins(gs, def);
    load_yellow_stars(gs, def);
    load_last_star(gs, def);

    /* Enemies */
    load_spiders(gs, def);
    load_jumping_spiders(gs, def);
    load_birds(gs, def);
    load_faster_birds(gs, def);
    load_fish(gs, def);
    load_faster_fish(gs, def);

    /* Hazards */
    load_axe_traps(gs, def);
    load_circular_saws(gs, def);
    load_spike_rows(gs, def);
    load_spike_platforms(gs, def);
    load_spike_blocks(gs, def);
    load_blue_flames(gs);

    /* Surfaces */
    load_float_platforms(gs, def);
    load_bridges(gs, def);
}
