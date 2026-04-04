/*
 * level.h — Data-driven level definition system.
 *
 * A level is described entirely as a LevelDef constant: arrays of placement
 * structs that specify WHERE every entity lives and what variant/mode it uses.
 * No positions are hard-coded inside entity .c files — they only know HOW
 * entities behave.
 *
 * Adding a new level = creating one new levels/level_XX.c file with a filled
 * LevelDef.  No engine code needs to change.
 *
 * Usage:
 *   #include "level.h"
 *   #include "levels/sandbox_00.h"     // extern const LevelDef sandbox_00_def;
 *   level_load(gs, &sandbox_00_def);   // from level_loader.h
 *   level_reset(gs, &sandbox_00_def);  // on player death
 */
#pragma once

#include <SDL.h>                       /* Uint32 */
#include "../surfaces/bouncepad.h"     /* BouncepadType, MAX_BOUNCEPADS_* */
#include "../hazards/axe_trap.h"       /* AxeTrapMode, MAX_AXE_TRAPS */
#include "../surfaces/float_platform.h"/* FloatPlatformMode, MAX_FLOAT_PLATFORMS */
#include "../surfaces/rail.h"          /* MAX_RAILS */
#include "../game.h"                   /* MAX_* constants, FLOOR_Y, TILE_SIZE, etc. */

/* ------------------------------------------------------------------ */
/* Rail placements                                                     */
/* ------------------------------------------------------------------ */

/*
 * RailLayout — selects which builder function constructs this rail.
 *
 * RAIL_LAYOUT_RECT  : closed rectangular loop.  Uses x, y, w, h.
 * RAIL_LAYOUT_HORIZ : horizontal open/closed line.  Uses x, y, w; ignores h.
 */
typedef enum {
    RAIL_LAYOUT_RECT,
    RAIL_LAYOUT_HORIZ
} RailLayout;

/*
 * RailPlacement — all parameters needed to construct one Rail path.
 *
 * layout   : selects RECT or HORIZ builder.
 * x, y     : world-space top-left corner of the rail (logical pixels).
 * w        : width in tiles for both RECT and HORIZ.
 * h        : height in tiles (RECT only; ignored for HORIZ).
 * end_cap  : HORIZ only — 1 = right end-cap tile (rider bounces back),
 *            0 = no cap (rider detaches and enters free-fall).
 *            RECT loops are always closed; this field is ignored.
 */
typedef struct {
    RailLayout layout;
    int x, y;
    int w, h;
    int end_cap;
} RailPlacement;

/* ------------------------------------------------------------------ */
/* Entity placement structs                                            */
/* ------------------------------------------------------------------ */

/*
 * PlatformPlacement — one pillar on the ground floor.
 *
 * x           : left edge in world-space logical pixels.
 * tile_height : number of TILE_SIZE (48 px) tiles tall (typically 2 or 3).
 */
typedef struct {
    float x;
    int   tile_height;
} PlatformPlacement;

/*
 * SpiderPlacement — one ground-patrol spider.
 *
 * x            : starting world-space x.
 * vx           : starting velocity (sign encodes direction; use ±SPIDER_SPEED).
 * patrol_x0/x1 : left and right patrol boundaries.
 * frame_index  : starting animation frame (0–2; vary for visual diversity).
 */
typedef struct {
    float x;
    float vx;
    float patrol_x0;
    float patrol_x1;
    int   frame_index;
} SpiderPlacement;

/*
 * JumpingSpiderPlacement — one jumping spider (crosses sea gaps).
 *
 * Same fields as SpiderPlacement; y is always derived from FLOOR_Y.
 */
typedef struct {
    float x;
    float vx;
    float patrol_x0;
    float patrol_x1;
} JumpingSpiderPlacement;

/*
 * BirdPlacement — one sky-patrol bird (slow or fast variant).
 *
 * base_y  : vertical centre of the sine-wave flight path.
 */
typedef struct {
    float x;
    float base_y;
    float vx;
    float patrol_x0;
    float patrol_x1;
    int   frame_index;
} BirdPlacement;

/*
 * FishPlacement — one water-lane jumping fish (slow or fast variant).
 *
 * x, vx, patrol_x0, patrol_x1: horizontal patrol data.
 * water_y is always computed from game constants (GAME_H, WATER_ART_H).
 */
typedef struct {
    float x;
    float vx;
    float patrol_x0;
    float patrol_x1;
} FishPlacement;

/*
 * CoinPlacement — one collectable coin.
 */
typedef struct {
    float x;
    float y;
} CoinPlacement;

/*
 * YellowStarPlacement — one health-restoring star pickup.
 */
typedef struct {
    float x;
    float y;
} YellowStarPlacement;

/*
 * LastStarPlacement — the single end-of-level star.
 */
typedef struct {
    float x;
    float y;
} LastStarPlacement;

/*
 * AxeTrapPlacement — one swinging / spinning axe.
 *
 * pillar_x : left edge of the host platform pillar in world pixels.
 *            The pivot is computed as: pillar_x + TILE_SIZE/2.
 * mode     : AXE_MODE_PENDULUM or AXE_MODE_SPIN.
 */
typedef struct {
    float      pillar_x;
    AxeTrapMode mode;
} AxeTrapPlacement;

/*
 * CircularSawPlacement — one horizontally patrolling rotating saw.
 *
 * y is computed: FLOOR_Y − 2 × TILE_SIZE + 16 − SAW_DISPLAY_H.
 */
typedef struct {
    float x;
    float patrol_x0;
    float patrol_x1;
    int   direction;   /* +1 = start moving right, -1 = start moving left */
} CircularSawPlacement;

/*
 * SpikeRowPlacement — a horizontal strip of ground spike tiles.
 *
 * y defaults to FLOOR_Y − SPIKE_TILE_H (on the floor surface).
 */
typedef struct {
    float x;
    int   count;       /* number of 16-px tiles */
} SpikeRowPlacement;

/*
 * SpikePlatformPlacement — an elevated spike hazard surface.
 *
 * tile_count : width in 16-px pieces (typically 3 for a 48-px span).
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} SpikePlatformPlacement;

/*
 * SpikeBlockPlacement — a rail-riding spike block.
 *
 * rail_index : index into the level's rails array (0-based).
 * t_offset   : starting position along the rail in tile units.
 * speed      : tiles per second (use SPIKE_SPEED_* constants).
 */
typedef struct {
    int   rail_index;
    float t_offset;
    float speed;
} SpikeBlockPlacement;

/*
 * FloatPlatformPlacement — a hovering / crumbling / rail-riding platform.
 *
 * mode       : FLOAT_PLATFORM_STATIC, CRUMBLE, or RAIL.
 * x, y       : starting position (ignored for RAIL mode; set by rail).
 * tile_count : platform width in 16-px pieces.
 * rail_index : index into rails array (RAIL mode only).
 * t_offset   : starting t position on the rail (RAIL mode only).
 * speed      : tiles per second along the rail (RAIL mode only).
 */
typedef struct {
    FloatPlatformMode mode;
    float             x;
    float             y;
    int               tile_count;
    int               rail_index;
    float             t_offset;
    float             speed;
} FloatPlatformPlacement;

/*
 * BridgePlacement — a tiled crumble walkway.
 *
 * brick_count : number of 16-px bricks in the bridge.
 */
typedef struct {
    float x;
    float y;
    int   brick_count;
} BridgePlacement;

/*
 * BouncepadPlacement — one bouncepad (any variant).
 *
 * pad_type   : BOUNCEPAD_GREEN / BOUNCEPAD_WOOD / BOUNCEPAD_RED.
 * launch_vy  : upward impulse (use BOUNCEPAD_VY_SMALL/MEDIUM/HIGH).
 * y defaults to FLOOR_Y − BOUNCEPAD_SRC_H.
 */
typedef struct {
    float         x;
    float         launch_vy;
    BouncepadType pad_type;
} BouncepadPlacement;

/*
 * VinePlacement — a hanging vine decoration on a platform.
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} VinePlacement;

/*
 * LadderPlacement — a climbable ladder decoration.
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} LadderPlacement;

/*
 * RopePlacement — a climbable rope decoration.
 */
typedef struct {
    float x;
    float y;
    int   tile_count;
} RopePlacement;

/* ------------------------------------------------------------------ */
/* LevelDef — the complete description of one level                    */
/* ------------------------------------------------------------------ */

/*
 * LevelDef — all data needed to load and reset a level.
 *
 * To create a new level, declare a const LevelDef in a new
 * src/levels/level_NN.c file and expose it via a header.
 * Then pass a pointer to level_load() / level_reset().
 *
 * All arrays use the same MAX_* upper bounds as GameState so the
 * level_loader can safely fill GameState arrays directly.
 *
 * Note on blue_flames: positions are derived automatically from the
 * sea_gaps array by blue_flames_init — no explicit placement struct needed.
 */
typedef struct {
    const char *name;   /* display name, e.g. "Sandbox" */

    /* ---- World geometry --------------------------------------------- */
    int sea_gaps[MAX_SEA_GAPS];
    int sea_gap_count;

    /* ---- Rails (must be initialised before spike_blocks/float_platforms) */
    RailPlacement   rails[MAX_RAILS];
    int             rail_count;

    /* ---- Static geometry -------------------------------------------- */
    PlatformPlacement platforms[MAX_PLATFORMS];
    int               platform_count;

    /* ---- Collectibles ------------------------------------------------ */
    CoinPlacement        coins[MAX_COINS];
    int                  coin_count;
    YellowStarPlacement  yellow_stars[MAX_YELLOW_STARS];
    int                  yellow_star_count;
    LastStarPlacement    last_star;

    /* ---- Enemies ----------------------------------------------------- */
    SpiderPlacement        spiders[MAX_SPIDERS];
    int                    spider_count;
    JumpingSpiderPlacement jumping_spiders[MAX_JUMPING_SPIDERS];
    int                    jumping_spider_count;
    BirdPlacement          birds[MAX_BIRDS];
    int                    bird_count;
    BirdPlacement          faster_birds[MAX_FASTER_BIRDS];
    int                    faster_bird_count;
    FishPlacement          fish[MAX_FISH];
    int                    fish_count;
    FishPlacement          faster_fish[MAX_FASTER_FISH];
    int                    faster_fish_count;

    /* ---- Hazards ----------------------------------------------------- */
    AxeTrapPlacement       axe_traps[MAX_AXE_TRAPS];
    int                    axe_trap_count;
    CircularSawPlacement   circular_saws[MAX_CIRCULAR_SAWS];
    int                    circular_saw_count;
    SpikeRowPlacement      spike_rows[MAX_SPIKE_ROWS];
    int                    spike_row_count;
    SpikePlatformPlacement spike_platforms[MAX_SPIKE_PLATFORMS];
    int                    spike_platform_count;
    SpikeBlockPlacement    spike_blocks[MAX_SPIKE_BLOCKS];
    int                    spike_block_count;

    /* ---- Surfaces ---------------------------------------------------- */
    FloatPlatformPlacement float_platforms[MAX_FLOAT_PLATFORMS];
    int                    float_platform_count;
    BridgePlacement        bridges[MAX_BRIDGES];
    int                    bridge_count;
    /*
     * Bouncepads are split by variant to match the three texture slots in
     * GameState (bouncepad_small_tex, bouncepad_medium_tex, bouncepad_high_tex).
     */
    BouncepadPlacement bouncepads_small[MAX_BOUNCEPADS_SMALL];
    int                bouncepad_small_count;
    BouncepadPlacement bouncepads_medium[MAX_BOUNCEPADS_MEDIUM];
    int                bouncepad_medium_count;
    BouncepadPlacement bouncepads_high[MAX_BOUNCEPADS_HIGH];
    int                bouncepad_high_count;

    /* ---- Decorations & climbables ------------------------------------ */
    VinePlacement   vines[MAX_VINES];
    int             vine_count;
    LadderPlacement ladders[MAX_LADDERS];
    int             ladder_count;
    RopePlacement   ropes[MAX_ROPES];
    int             rope_count;

    /* ---- Level-wide configuration ----------------------------------- */
    /*
     * Fields below control systems that were previously hardcoded in game.c.
     * Moving them into LevelDef lets the editor configure them per-level.
     */

    /* Parallax background layers (back-to-front render order) */
    struct {
        char  path[64];   /* PNG path relative to repo root */
        float speed;      /* scroll fraction: 0.0=static, 0.5=half cam speed */
    } parallax_layers[PARALLAX_MAX_LAYERS];
    int parallax_layer_count;

    /* Player spawn position */
    float player_start_x;
    float player_start_y;

    /* Background music */
    char  music_path[64];   /* path to .ogg/.wav music file, empty = no music */
    int   music_volume;     /* 0-128, SDL_mixer range */

    /* Fog atmosphere */
    int   fog_enabled;      /* 1 = fog active, 0 = disabled */
} LevelDef;

/* ------------------------------------------------------------------ */
/* Rail builder — declared here so RailPlacement is fully visible      */
/* ------------------------------------------------------------------ */

/*
 * rail_init_from_placements — Build rail paths from a RailPlacement array.
 *
 * Implemented in rail.c.  Declared here (instead of rail.h) because the
 * parameter type RailPlacement is only defined in this header.
 */
void rail_init_from_placements(Rail *rails, int *count,
                               const RailPlacement *placements, int n);
