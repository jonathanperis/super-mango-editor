/*
 * sandbox.c — Sandbox level: entity placements and level layout.
 *
 * This file defines WHERE every entity lives in the sandbox phase.
 * The game engine (game.c) handles HOW they behave (loop, collisions,
 * rendering).  Separating content from engine allows future phases
 * to swap in different layouts without touching the engine.
 */

#include "sandbox.h"
#include "game.h"

/* ------------------------------------------------------------------ */

/*
 * sandbox_load_level — Place all entities and define the level geometry.
 *
 * Called once from game_init after all textures are loaded and the
 * renderer is ready.  Each entity's own _init function contains the
 * specific world-space positions for this sandbox level.
 */
void sandbox_load_level(GameState *gs) {
    /*
     * Sea gaps — holes in the ground floor that expose the water below.
     * Must be set before blue_flames_init because blue flames read gap positions.
     */
    gs->sea_gaps[0] = 0;       /* left world edge  (screen 1 start)   */
    gs->sea_gaps[1] = 192;     /* screen 1 mid     (free zone 187–255)*/
    gs->sea_gaps[2] = 560;     /* screen 2 mid     (free zone 549–594)*/
    gs->sea_gaps[3] = 928;     /* screen 3 mid     (16px-aligned)     */
    gs->sea_gaps[4] = 1152;    /* screen 3–4 boundary                 */
    gs->sea_gap_count = 5;

    /* ---- Static level geometry ----------------------------------- */
    platforms_init(gs->platforms, &gs->platform_count);
    rail_init(gs->rails, &gs->rail_count);

    /* ---- Collectibles -------------------------------------------- */
    coins_init(gs->coins, &gs->coin_count);
    yellow_stars_init(gs->yellow_stars, &gs->yellow_star_count);
    last_star_init(&gs->last_star);

    /* ---- Enemies ------------------------------------------------- */
    spiders_init(gs->spiders, &gs->spider_count);
    jumping_spiders_init(gs->jumping_spiders, &gs->jumping_spider_count);
    birds_init(gs->birds, &gs->bird_count);
    faster_birds_init(gs->faster_birds, &gs->faster_bird_count);
    fish_init(gs->fish, &gs->fish_count);
    faster_fish_init(gs->faster_fish, &gs->faster_fish_count);

    /* ---- Hazards ------------------------------------------------- */
    spike_blocks_init(gs->spike_blocks, &gs->spike_block_count, gs->rails);
    axe_traps_init(gs->axe_traps, &gs->axe_trap_count);
    circular_saws_init(gs->circular_saws, &gs->circular_saw_count);
    blue_flames_init(gs->blue_flames, &gs->blue_flame_count,
                gs->sea_gaps, gs->sea_gap_count);
    spike_rows_init(gs->spike_rows, &gs->spike_row_count);
    spike_platforms_init(gs->spike_platforms, &gs->spike_platform_count);

    /* ---- Platforms & surfaces ------------------------------------ */
    float_platforms_init(gs->float_platforms, &gs->float_platform_count, gs->rails);
    bridges_init(gs->bridges, &gs->bridge_count);
    bouncepads_medium_init(gs->bouncepads_medium, &gs->bouncepad_medium_count);
    bouncepads_small_init(gs->bouncepads_small, &gs->bouncepad_small_count);
    bouncepads_high_init(gs->bouncepads_high, &gs->bouncepad_high_count);

    /* ---- Decorations & climbables -------------------------------- */
    vine_init(gs->vines, &gs->vine_count);
    ladder_init(gs->ladders, &gs->ladder_count);
    rope_init(gs->ropes, &gs->rope_count);
}

/* ------------------------------------------------------------------ */

/*
 * sandbox_reset_level — Reinitialise all entities after a player death.
 *
 * Restores every entity array to its initial state so the level plays
 * identically each time.  The player is also reset to the spawn point.
 */
void sandbox_reset_level(GameState *gs, int *fp_prev_riding) {
    player_reset(&gs->player);

    /* Collectibles */
    coins_init(gs->coins, &gs->coin_count);
    yellow_stars_init(gs->yellow_stars, &gs->yellow_star_count);
    last_star_init(&gs->last_star);

    /* Enemies */
    spiders_init(gs->spiders, &gs->spider_count);
    jumping_spiders_init(gs->jumping_spiders, &gs->jumping_spider_count);
    birds_init(gs->birds, &gs->bird_count);
    faster_birds_init(gs->faster_birds, &gs->faster_bird_count);
    fish_init(gs->fish, &gs->fish_count);
    faster_fish_init(gs->faster_fish, &gs->faster_fish_count);

    /* Hazards */
    spike_blocks_init(gs->spike_blocks, &gs->spike_block_count, gs->rails);
    axe_traps_init(gs->axe_traps, &gs->axe_trap_count);
    circular_saws_init(gs->circular_saws, &gs->circular_saw_count);
    blue_flames_init(gs->blue_flames, &gs->blue_flame_count,
                gs->sea_gaps, gs->sea_gap_count);
    spike_rows_init(gs->spike_rows, &gs->spike_row_count);
    spike_platforms_init(gs->spike_platforms, &gs->spike_platform_count);

    /* Platforms & surfaces */
    float_platforms_init(gs->float_platforms, &gs->float_platform_count, gs->rails);
    bridges_init(gs->bridges, &gs->bridge_count);

    /* Climbables */
    ladder_init(gs->ladders, &gs->ladder_count);
    rope_init(gs->ropes, &gs->rope_count);

    *fp_prev_riding = -1;
}
