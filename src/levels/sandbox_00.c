#include "sandbox_00.h"

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

const LevelDef sandbox_00_def = {
    .name = "Sandbox",

    /* ---- Sea gaps ---- */
    .sea_gaps      = { 0, 192, 560, 928, 1152, 480, 448 },
    .sea_gap_count = 7,

    /* ---- Rails ---- */
    .rails = {
        { RAIL_LAYOUT_RECT, .x = 444, .y = 35, .w = 10, .h = 6, .end_cap = 0 },
        { RAIL_LAYOUT_RECT, .x = 852, .y = 50, .w = 8, .h = 5, .end_cap = 0 },
        { RAIL_LAYOUT_HORIZ, .x = 1200, .y = 112, .w = 14, .h = 0, .end_cap = 0 },
    },
    .rail_count = 3,

    /* ---- Platforms ---- */
    .platforms = {
        { .x = 80.0f, .tile_height = 2 },
        { .x = 256.0f, .tile_height = 3 },
        { .x = 452.0f, .tile_height = 2 },
        { .x = 680.0f, .tile_height = 3 },
        { .x = 880.0f, .tile_height = 2 },
        { .x = 1050.0f, .tile_height = 3 },
        { .x = 1300.0f, .tile_height = 2 },
        { .x = 1480.0f, .tile_height = 2 },
    },
    .platform_count = 8,

    /* ---- Coins ---- */
    .coins = {
        { 46.0f, 236.0f },
        { 170.0f, 236.0f },
        { 368.0f, 236.0f },
        { 272.0f, 108.0f },
        { 430.0f, 236.0f },
        { 595.0f, 236.0f },
        { 904.0f, 236.0f },
        { 468.0f, 156.0f },
        { 855.0f, 236.0f },
        { 965.0f, 236.0f },
        { 1130.0f, 236.0f },
        { 896.0f, 156.0f },
        { 1230.0f, 236.0f },
        { 1390.0f, 236.0f },
        { 1316.0f, 156.0f },
        { 1496.0f, 156.0f },
    },
    .coin_count = 16,

    /* ---- Yellow stars ---- */
    .yellow_stars = {
        { 272.0f, 108.0f },
        { 196.0f, 184.0f },
        { 696.0f, 108.0f },
    },
    .yellow_star_count = 3,

    /* ---- Last star ---- */
    .last_star = { .x = 1492.0f, .y = 100.0f },

    /* ---- Spiders ---- */
    .spiders = {
        { .x = 600.0f, .vx = 50.0f,
          .patrol_x0 = 592.0f, .patrol_x1 = 750.0f, .frame_index = 0 },
        { .x = 1100.0f, .vx = -50.0f,
          .patrol_x0 = 1000.0f, .patrol_x1 = 1152.0f, .frame_index = 1 },
    },
    .spider_count = 2,

    /* ---- Jumping spiders ---- */
    .jumping_spiders = {
        { .x = 130.0f, .vx = 55.0f,
          .patrol_x0 = 46.0f, .patrol_x1 = 310.0f },
    },
    .jumping_spider_count = 1,

    /* ---- Birds ---- */
    .birds = {
        { .x = 100.0f, .base_y = 60.0f, .vx = 45.0f,
          .patrol_x0 = 0.0f, .patrol_x1 = 700.0f, .frame_index = 0 },
        { .x = 1100.0f, .base_y = 80.0f, .vx = -45.0f,
          .patrol_x0 = 800.0f, .patrol_x1 = 1600.0f, .frame_index = 1 },
    },
    .bird_count = 2,

    /* ---- Faster birds ---- */
    .faster_birds = {
        { .x = 600.0f, .base_y = 50.0f, .vx = -80.0f,
          .patrol_x0 = 300.0f, .patrol_x1 = 1100.0f, .frame_index = 0 },
        { .x = 1200.0f, .base_y = 40.0f, .vx = 80.0f,
          .patrol_x0 = 900.0f, .patrol_x1 = 1600.0f, .frame_index = 2 },
    },
    .faster_bird_count = 2,

    /* ---- Fish ---- */
    .fish = {
        { .x = 700.0f, .vx = 70.0f,
          .patrol_x0 = 500.0f, .patrol_x1 = 950.0f },
        { .x = 1200.0f, .vx = -70.0f,
          .patrol_x0 = 1000.0f, .patrol_x1 = 1500.0f },
    },
    .fish_count = 2,

    /* ---- Faster fish ---- */
    .faster_fish = {
        { .x = 1100.0f, .vx = 120.0f,
          .patrol_x0 = 900.0f, .patrol_x1 = 1400.0f },
    },
    .faster_fish_count = 1,

    /* ---- Axe traps ---- */
    .axe_traps = {
        { .pillar_x = 256.0f, .mode = AXE_MODE_PENDULUM },
        { .pillar_x = 680.0f, .mode = AXE_MODE_SPIN },
    },
    .axe_trap_count = 2,

    /* ---- Circular saws ---- */
    .circular_saws = {
        { .x = 1350.0f, .patrol_x0 = 1350.0f, .patrol_x1 = 1446.0f, .direction = 1 },
    },
    .circular_saw_count = 1,

    /* ---- Spike rows ---- */
    .spike_rows = {
        { .x = 780.0f, .count = 4 },
    },
    .spike_row_count = 1,

    /* ---- Spike platforms ---- */
    .spike_platforms = {
        { .x = 370.0f, .y = 200.0f, .tile_count = 3 },
    },
    .spike_platform_count = 1,

    /* ---- Spike blocks ---- */
    .spike_blocks = {
        { .rail_index = 0, .t_offset = 0.0f, .speed = 1.5f },
        { .rail_index = 1, .t_offset = 11.0f, .speed = 3.0f },
        { .rail_index = 2, .t_offset = 0.0f, .speed = 6.0f },
    },
    .spike_block_count = 3,

    /* ---- Float platforms ---- */
    .float_platforms = {
        { .mode = FLOAT_PLATFORM_STATIC, .x = 172.0f, .y = 200.0f, .tile_count = 4,
          .rail_index = 0, .t_offset = 0.0f, .speed = 0.0f },
        { .mode = FLOAT_PLATFORM_CRUMBLE, .x = 570.0f, .y = 190.0f, .tile_count = 3,
          .rail_index = 0, .t_offset = 0.0f, .speed = 0.0f },
        { .mode = FLOAT_PLATFORM_RAIL, .x = 0.0f, .y = 0.0f, .tile_count = 3,
          .rail_index = 1, .t_offset = 14.0f, .speed = 2.0f },
    },
    .float_platform_count = 3,

    /* ---- Bridges ---- */
    .bridges = {
        { .x = 1350.0f, .y = 172.0f, .brick_count = 8 },
    },
    .bridge_count = 1,

    /* ---- Bouncepads small ---- */
    .bouncepads_small = {
        { .x = 734.0f, .launch_vy = -380.0f, .pad_type = BOUNCEPAD_GREEN },
    },
    .bouncepad_small_count = 1,

    /* ---- Bouncepads medium ---- */
    .bouncepads_medium = {
        { .x = 310.0f, .launch_vy = -536.2f, .pad_type = BOUNCEPAD_WOOD },
    },
    .bouncepad_medium_count = 1,

    /* ---- Bouncepads high ---- */
    .bouncepads_high = {
        { .x = 1420.0f, .launch_vy = -700.0f, .pad_type = BOUNCEPAD_RED },
    },
    .bouncepad_high_count = 1,

    /* ---- Vines ---- */
    .vines = {
        { .x = 88.0f, .y = 172.0f, .tile_count = 2 },
        { .x = 280.0f, .y = 124.0f, .tile_count = 3 },
        { .x = 688.0f, .y = 124.0f, .tile_count = 3 },
        { .x = 1074.0f, .y = 124.0f, .tile_count = 3 },
        { .x = 1308.0f, .y = 172.0f, .tile_count = 2 },
    },
    .vine_count = 5,

    /* ---- Ladders ---- */
    .ladders = {
        { .x = 1552.0f, .y = -962.0f, .tile_count = 150 },
    },
    .ladder_count = 1,

    /* ---- Ropes ---- */
    .ropes = {
        { .x = 460.0f, .y = 172.0f, .tile_count = 1 },
    },
    .rope_count = 1,

    /* ---- Level-wide configuration ----------------------------------- */

    /* Parallax layers (back-to-front) — 7 layers for the forest theme */
    .parallax_layers = {
        { "assets/parallax_sky.png",               0.00f },
        { "assets/parallax_clouds_bg.png",         0.08f },
        { "assets/parallax_glacial_mountains.png", 0.15f },
        { "assets/parallax_clouds_mg_3.png",       0.25f },
        { "assets/parallax_clouds_mg_2.png",       0.38f },
        { "assets/parallax_cloud_lonely.png",      0.44f },
        { "assets/parallax_clouds_mg_1.png",       0.50f },
    },
    .parallax_layer_count = 7,

    /* Player spawns on top of the first 2-tile platform at x=80 */
    .player_start_x = 80.0f,
    .player_start_y = 172.0f,

    /* Background music */
    .music_path   = "sounds/game_music.ogg",
    .music_volume = 13,

    /* Fog enabled for atmospheric forest effect */
    .fog_enabled = 1,
};
