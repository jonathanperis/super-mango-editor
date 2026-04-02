/*
 * level_01.c — Sandbox level: the original hand-crafted four-screen level.
 *
 * This file contains ONLY placement data — positions, patrol ranges, modes.
 * Entity behaviour lives in each entity's .c module.
 * The bridge between this data and the GameState is level_loader.c.
 *
 * All y coordinates are in logical pixels (0 = top, GAME_H-1 = bottom).
 * Key y reference points:
 *   FLOOR_Y                      = 252   (top of floor tile)
 *   FLOOR_Y - 1*TILE_SIZE + 16   = 220   (1-tile pillar top)
 *   FLOOR_Y - 2*TILE_SIZE + 16   = 172   (2-tile pillar top)
 *   FLOOR_Y - 3*TILE_SIZE + 16   = 124   (3-tile pillar top)
 */

#include "level_01.h"

/* Pull in entity constants needed for SPEED / VY / MODE values */
#include "../entities/spider.h"         /* SPIDER_SPEED */
#include "../entities/jumping_spider.h" /* JSPIDER_SPEED */
#include "../entities/bird.h"           /* BIRD_SPEED */
#include "../entities/faster_bird.h"    /* FBIRD_SPEED */
#include "../entities/fish.h"           /* FISH_SPEED */
#include "../entities/faster_fish.h"    /* FFISH_SPEED */
#include "../collectibles/coin.h"       /* COIN_DISPLAY_W, COIN_DISPLAY_H */
#include "../collectibles/yellow_star.h"/* YELLOW_STAR_DISPLAY_W/H */
#include "../collectibles/last_star.h"  /* LAST_STAR_DISPLAY_W/H */
#include "../surfaces/bouncepad.h"      /* BOUNCEPAD_VY_*, BouncepadType */
#include "../hazards/axe_trap.h"        /* AxeTrapMode */
#include "../hazards/circular_saw.h"    /* SAW_DISPLAY_W/H */
#include "../hazards/spike.h"           /* SPIKE_TILE_H */
#include "../hazards/spike_block.h"     /* SPIKE_SPEED_* */
#include "../surfaces/float_platform.h" /* FLOAT_PLATFORM_*, CRUMBLE_STAND_LIMIT */
#include "../surfaces/vine.h"           /* VINE_W */
#include "../game.h"                    /* FLOOR_Y, TILE_SIZE, GAME_H */

/* ------------------------------------------------------------------ */

const LevelDef level_01_def = {
    .name = "Sandbox",

    /* ---- Sea gaps: holes in the ground floor exposing water below -- */
    /* Must be set before blue_flames — their positions derive from here */
    .sea_gaps       = { 0, 192, 560, 928, 1152 },
    .sea_gap_count  = 5,

    /* ---- Rails: closed loops and open lines used by spike blocks ---- */
    /*
     * Rail 0 — 10×6-tile closed rectangle, screen 2.
     *   Bottom edge at y = 35 + 5×16 = 115, clearing 2-tile platform coins.
     * Rail 1 — 8×5-tile closed rectangle, screen 3.
     *   Bottom edge at y = 50 + 4×16 = 114, clearing 2-tile platform coins.
     * Rail 2 — 14-tile open horizontal line, screen 4.
     *   No right end-cap: fast spike block detaches and free-falls at the end.
     */
    .rails = {
        { RAIL_LAYOUT_RECT,  .x = 444, .y =  35, .w = 10, .h = 6, .end_cap = 0 },
        { RAIL_LAYOUT_RECT,  .x = 852, .y =  50, .w =  8, .h = 5, .end_cap = 0 },
        { RAIL_LAYOUT_HORIZ, .x =1200, .y = 112, .w = 14, .h = 0, .end_cap = 0 },
    },
    .rail_count = 3,

    /* ---- Static geometry: pillars across four screens --------------- */
    /*
     * Two pillars per screen: one 2-tile (medium) and one 3-tile (tall).
     * Pillar y = FLOOR_Y − tile_height × TILE_SIZE + 16  (embedded 16 px into floor).
     */
    .platforms = {
        { .x =   80, .tile_height = 2 },  /* screen 1, medium */
        { .x =  256, .tile_height = 3 },  /* screen 1, tall   */
        { .x =  452, .tile_height = 2 },  /* screen 2, medium */
        { .x =  680, .tile_height = 3 },  /* screen 2, tall   */
        { .x =  880, .tile_height = 2 },  /* screen 3, medium */
        { .x = 1050, .tile_height = 3 },  /* screen 3, tall   */
        { .x = 1300, .tile_height = 2 },  /* screen 4, medium */
        { .x = 1480, .tile_height = 2 },  /* screen 4, medium */
    },
    .platform_count = 8,

    /* ---- Collectibles ----------------------------------------------- */
    /*
     * y values:
     *   gy = FLOOR_Y - COIN_DISPLAY_H        = 252 - 16 = 236  (ground coins)
     *   p2 = FLOOR_Y - 2*TILE + 16 - C_H     = 252 - 96 + 16 - 16 = 156  (2-tile top)
     *   p3 = FLOOR_Y - 3*TILE + 16 - C_H     = 252 - 144 + 16 - 16 = 108 (3-tile top)
     */
    .coins = {
        /* Screen 1 — ground */
        {   46.0f, 236.0f }, {  170.0f, 236.0f }, {  368.0f, 236.0f },
        /* Screen 1 — 3-tile pillar top (x=256, centred) */
        {  272.0f, 108.0f },
        /* Screen 2 — ground */
        {  430.0f, 236.0f }, {  595.0f, 236.0f }, {  904.0f, 236.0f },
        /* Screen 2 — 2-tile pillar top (x=452, centred: 452+24-8=468) */
        {  468.0f, 156.0f },
        /* Screen 3 — ground */
        {  855.0f, 236.0f }, {  965.0f, 236.0f }, { 1130.0f, 236.0f },
        /* Screen 3 — 2-tile pillar top (x=880, centred: 880+24-8=896) */
        {  896.0f, 156.0f },
        /* Screen 4 — ground */
        { 1230.0f, 236.0f }, { 1390.0f, 236.0f },
        /* Screen 4 — 2-tile pillar tops (x=1300→1316, x=1480→1496) */
        { 1316.0f, 156.0f }, { 1496.0f, 156.0f },
    },
    .coin_count = 16,

    /*
     * Yellow stars: health restores.
     *   Star 0 — 3-tile pillar top at x=256: (256+24-8=272, 108)
     *   Star 1 — static float platform at x=172,y=200: (172+32-8=196, 200-16=184)
     *   Star 2 — 3-tile pillar top at x=680: (680+24-8=696, 108)
     */
    .yellow_stars = {
        {  272.0f, 108.0f },
        {  196.0f, 184.0f },
        {  696.0f, 108.0f },
    },
    .yellow_star_count = 3,

    /*
     * Last star — end-of-level prize, atop the final tall column.
     *   x = 1480 + (48 - 24) / 2 = 1492
     *   y = FLOOR_Y - 3*TILE + 16 - LAST_STAR_DISPLAY_H = 252-144+16-24 = 100
     */
    .last_star = { .x = 1492.0f, .y = 100.0f },

    /* ---- Enemies ---------------------------------------------------- */

    /* Ground-patrol spiders */
    .spiders = {
        /* Spider 0 — screens 2–3, east of gap at x=560–592 */
        { .x = 600.0f, .vx =  SPIDER_SPEED,
          .patrol_x0 = 592.0f, .patrol_x1 = 750.0f, .frame_index = 0 },
        /* Spider 1 — screens 3–4, west of gap at x=1152–1184 */
        { .x = 1100.0f, .vx = -SPIDER_SPEED,
          .patrol_x0 = 1000.0f, .patrol_x1 = 1152.0f, .frame_index = 1 },
    },
    .spider_count = 2,

    /* Jumping spiders — cross sea gaps on the ground floor */
    .jumping_spiders = {
        /* JS 0 — screen 1, crosses gap at x=192–224; patrol 46–310 */
        { .x = 130.0f, .vx = JSPIDER_SPEED,
          .patrol_x0 = 46.0f, .patrol_x1 = 310.0f },
    },
    .jumping_spider_count = 1,

    /* Slow sine-wave birds */
    .birds = {
        /* Bird 0 — screens 1–2, upper sky */
        { .x = 100.0f, .base_y =  60.0f, .vx =  BIRD_SPEED,
          .patrol_x0 =   0.0f, .patrol_x1 =  700.0f, .frame_index = 0 },
        /* Bird 1 — screens 3–4, slightly lower, starts going left */
        { .x =1100.0f, .base_y =  80.0f, .vx = -BIRD_SPEED,
          .patrol_x0 = 800.0f, .patrol_x1 = 1600.0f, .frame_index = 1 },
    },
    .bird_count = 2,

    /* Fast sine-wave birds */
    .faster_birds = {
        /* FBird 0 — screens 2–3, starts going left */
        { .x =  600.0f, .base_y = 50.0f, .vx = -FBIRD_SPEED,
          .patrol_x0 =  300.0f, .patrol_x1 = 1100.0f, .frame_index = 0 },
        /* FBird 1 — screens 3–4, starts going right */
        { .x = 1200.0f, .base_y = 40.0f, .vx =  FBIRD_SPEED,
          .patrol_x0 =  900.0f, .patrol_x1 = 1600.0f, .frame_index = 2 },
    },
    .faster_bird_count = 2,

    /* Jumping fish — patrol underwater, leap onto ground floor */
    .fish = {
        /* Fish 0 — screens 2–3 */
        { .x =  700.0f, .vx =  FISH_SPEED,
          .patrol_x0 =  500.0f, .patrol_x1 =  950.0f },
        /* Fish 1 — screens 3–4, starts going left */
        { .x = 1200.0f, .vx = -FISH_SPEED,
          .patrol_x0 = 1000.0f, .patrol_x1 = 1500.0f },
    },
    .fish_count = 2,

    /* Faster fish */
    .faster_fish = {
        /* FFish 0 — screens 3–4 */
        { .x = 1100.0f, .vx = FFISH_SPEED,
          .patrol_x0 = 900.0f, .patrol_x1 = 1400.0f },
    },
    .faster_fish_count = 1,

    /* ---- Hazards ---------------------------------------------------- */

    /* Axe traps: each hangs from the top centre of a 3-tile pillar */
    .axe_traps = {
        /* Trap 0 — pendulum, pillar at x=256 (screen 1) */
        { .pillar_x = 256.0f, .mode = AXE_MODE_PENDULUM },
        /* Trap 1 — full spin, pillar at x=680 (screen 2) */
        { .pillar_x = 680.0f, .mode = AXE_MODE_SPIN },
    },
    .axe_trap_count = 2,

    /* Circular saws: patrol a platform-height horizontal surface */
    .circular_saws = {
        /*
         * Saw 0 — patrols atop the bridge surface (screen 4).
         * y = FLOOR_Y − 2×TILE + 16 − SAW_H = 252 − 96 + 16 − 32 = 140.
         * patrol_x1 = 1478 − SAW_DISPLAY_W = 1446.
         */
        { .x = 1350.0f, .patrol_x0 = 1350.0f, .patrol_x1 = 1446.0f, .direction = 1 },
    },
    .circular_saw_count = 1,

    /* Spike rows: horizontal strips of static 16×16 ground spikes */
    .spike_rows = {
        /* Row 0 — screen 3, 4 tiles × 16 px = 64 px, just before the gap */
        { .x = 780.0f, .count = 4 },
    },
    .spike_row_count = 1,

    /* Spike platforms: elevated hazard surfaces */
    .spike_platforms = {
        /* SP 0 — screen 1–2 gap, 3 pieces = 48 px wide */
        { .x = 370.0f, .y = 200.0f, .tile_count = 3 },
    },
    .spike_platform_count = 1,

    /*
     * Spike blocks: rail riders.
     * t_offset for block 1: rail[1] is a 8×5-tile RECT = 22 tiles total.
     * Starting at tile 11 (half loop) ensures blocks 0 and 1 are never synced.
     */
    .spike_blocks = {
        { .rail_index = 0, .t_offset =  0.0f, .speed = SPIKE_SPEED_SLOW   },
        { .rail_index = 1, .t_offset = 11.0f, .speed = SPIKE_SPEED_NORMAL },
        { .rail_index = 2, .t_offset =  0.0f, .speed = SPIKE_SPEED_FAST   },
    },
    .spike_block_count = 3,

    /* ---- Surfaces --------------------------------------------------- */

    /* Floating / crumbling / rail-riding platforms */
    .float_platforms = {
        /* FP 0 — static hover, screen 1 */
        { .mode = FLOAT_PLATFORM_STATIC, .x = 172.0f, .y = 200.0f, .tile_count = 4,
          .rail_index = 0, .t_offset = 0.0f, .speed = 0.0f },
        /* FP 1 — crumble, screen 2, right of rail 0 */
        { .mode = FLOAT_PLATFORM_CRUMBLE, .x = 570.0f, .y = 190.0f, .tile_count = 3,
          .rail_index = 0, .t_offset = 0.0f, .speed = 0.0f },
        /* FP 2 — rail-riding, screen 3, on rail 1, starts near bottom of loop */
        { .mode = FLOAT_PLATFORM_RAIL, .x = 0.0f, .y = 0.0f, .tile_count = 3,
          .rail_index = 1, .t_offset = 14.0f, .speed = 2.0f },
    },
    .float_platform_count = 3,

    /* Tiled crumble bridges */
    .bridges = {
        /*
         * Bridge 0 — 8 bricks between the last two pillars (screen 4).
         * x=1350, y=FLOOR_Y−2×TILE+16=172, spans 8×16=128 px of the 132-px gap.
         */
        { .x = 1350.0f, .y = 172.0f, .brick_count = 8 },
    },
    .bridge_count = 1,

    /* Bouncepads — one per variant (small/medium/high) */
    .bouncepads_medium = {
        /* Wood pad — screen 1, right of 3-tile pillar at x=256 */
        { .x = 310.0f, .launch_vy = BOUNCEPAD_VY_MEDIUM, .pad_type = BOUNCEPAD_WOOD },
    },
    .bouncepad_medium_count = 1,
    .bouncepads_small = {
        /* Green pad — screen 2, right of tall pillar at x=680 */
        { .x = 734.0f, .launch_vy = BOUNCEPAD_VY_SMALL, .pad_type = BOUNCEPAD_GREEN },
    },
    .bouncepad_small_count = 1,
    .bouncepads_high = {
        /* Red pad — screen 4, left of last tall pillar at x=1480 */
        { .x = 1420.0f, .launch_vy = BOUNCEPAD_VY_HIGH, .pad_type = BOUNCEPAD_RED },
    },
    .bouncepad_high_count = 1,

    /* ---- Decorations & climbables ---------------------------------- */

    /*
     * Vines on platform left/right edges.
     * x formula: left side  = pillar_x + 8          (VINE_BORDER = 8)
     *            right side = pillar_x + 48 - 8 - 16 (TILE_SIZE - BORDER - VINE_W)
     */
    .vines = {
        { .x =   88.0f, .y = 172.0f, .tile_count = 2 }, /* pillar 0 left  (x=80)   */
        { .x =  280.0f, .y = 124.0f, .tile_count = 3 }, /* pillar 1 right (x=256)  */
        { .x =  688.0f, .y = 124.0f, .tile_count = 3 }, /* pillar 3 left  (x=680)  */
        { .x = 1074.0f, .y = 124.0f, .tile_count = 3 }, /* pillar 5 right (x=1050) */
        { .x = 1308.0f, .y = 172.0f, .tile_count = 2 }, /* pillar 6 left  (x=1300) */
    },
    .vine_count = 5,

    /* Ladder at the far end of the level — the player's exit */
    .ladders = {
        /*
         * x = 1528 (pillar 7 right edge) + 24 = 1552
         * y = FLOOR_Y - (149 × LADDER_STEP + LADDER_H) = 252 - (1192 + 22) = -962
         * tile_count = 150 (extends far above screen; shows infinity effect)
         */
        { .x = 1552.0f, .y = -962.0f, .tile_count = 150 },
    },
    .ladder_count = 1,

    /* Single rope on the 2-tile pillar at x=452 (screen 2) */
    .ropes = {
        /* x = 452 + 8 = 460; y = 2-tile pillar top = 172 */
        { .x = 460.0f, .y = 172.0f, .tile_count = 1 },
    },
    .rope_count = 1,
};
