/*
 * game.h — Public interface for the game module.
 *
 * Defines:
 *   - The compile-time constants shared across all files, by including
 *     game_constants.h (files that need only those include it directly).
 *   - The GameState struct that holds everything the game needs.
 *   - Declarations for the three functions that drive the game.
 */

/*
 * #pragma once is a non-standard but universally supported header guard.
 * It tells the compiler: "include this file only once per translation unit,
 * even if #included multiple times". Prevents duplicate-definition errors.
 */
#pragma once

/*
 * Why one big GameState (and so many includes here)?
 *
 * This project deliberately keeps every piece of live game data in one
 * struct, stored by value: entity arrays, counts, timers, camera, input
 * latches. A learner can find any value by following gs->..., the whole
 * game can be reset or replaced by working on one object, and there is no
 * hidden global state or heap graph to trace.
 *
 * The cost is visible below: to embed `Spider spiders[MAX_SPIDERS]` the
 * compiler must know sizeof(Spider), so game.h has to include every header
 * that defines a struct or MAX_* constant used by GameState. Those, and
 * game_constants.h, are the ONLY includes allowed here. Helpers a .c file merely calls (clock_millis,
 * str_copy from shared/platform.h, collision helpers, ...) are included by
 * that .c file itself, so its dependencies stay visible where they are used.
 */

#include "shared/graphics.h"        /* Texture2D, RenderTexture2D, IntRect */
#include "shared/audio.h"           /* SoundEffect, MusicTrack pointers */
#include <stdint.h>                 /* uint32_t, uint64_t fields */

#include "player/player.h"          /* Player struct — embedded by value in GameState */
#include "surfaces/platform.h"      /* Platform struct + MAX_PLATFORMS constant */
#include "effects/water.h"          /* Water struct — animated bottom strip */
#include "effects/fog.h"            /* FogSystem struct — atmospheric fog overlay */
#include "entities/spider.h"        /* Spider struct + MAX_SPIDERS constant */
#include "entities/fish.h"          /* Fish struct + MAX_FISH constant */
#include "collectibles/coin.h"      /* Coin struct + MAX_COINS constant */
#include "surfaces/vine.h"          /* VineDecor struct + MAX_VINES constant */
#include "surfaces/bouncepad.h"       /* Bouncepad struct — shared mechanics */
#include "surfaces/bouncepad_small.h" /* Green bouncepad (small jump) placement */
#include "surfaces/bouncepad_medium.h"/* Wood bouncepad (medium jump) placement */
#include "surfaces/bouncepad_high.h"  /* Red bouncepad (high jump) placement */
#include "screens/hud.h"            /* Hud struct — HUD display resources */
#include "effects/parallax.h"       /* ParallaxSystem — multi-layer scrolling background */
#include "surfaces/rail.h"          /* Rail, RailTile — rail path system */
#include "hazards/spike_block.h"    /* SpikeBlock — rail-riding hazard entity */
#include "surfaces/float_platform.h"/* FloatPlatform — hovering/crumble/rail surfaces */
#include "surfaces/bridge.h"        /* Bridge — tiled crumble walkway */
#include "entities/jumping_spider.h"/* JumpingSpider — jumping patrol enemy */
#include "entities/bird.h"          /* Bird — slow sine-wave sky patrol */
#include "entities/faster_bird.h"   /* FasterBird — fast sine-wave sky patrol */
#include "collectibles/health_star.h"/* HealthStar — yellow/green/red heart pickups */
#include "hazards/axe_trap.h"       /* AxeTrap — swinging/spinning axe hazard */
#include "hazards/circular_saw.h"   /* CircularSaw — fast rotating patrol hazard */
#include "hazards/blue_flame.h"     /* BlueFlame — erupting fire hazard from sea gaps */
#include "surfaces/ladder.h"        /* LadderDecor — climbable ladder */
#include "surfaces/rope.h"          /* RopeDecor — climbable rope */
#include "entities/faster_fish.h"   /* FasterFish — fast variant of jumping fish */
#include "collectibles/last_star.h" /* LastStar — end-of-level collectible */
#include "hazards/spike.h"          /* SpikeRow — static ground spike hazards */
#include "hazards/spike_platform.h" /* SpikePlatform — elevated spike hazard surface */
#include "core/debug.h"             /* DebugOverlay — debug collision/FPS/log overlay */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/*
 * Window size, logical resolution, TILE_SIZE, FLOOR_Y, GRAVITY, floor-gap
 * and level limits, and camera tuning live in game_constants.h, so files
 * that need only those numbers can include it without the rest of this
 * header.
 */
#include "game_constants.h"

typedef enum {
    CHECKPOINT_FEEDBACK_NONE = 0,
    CHECKPOINT_FEEDBACK_SAVED,
    CHECKPOINT_FEEDBACK_RESPAWN
} CheckpointFeedbackKind;

/* ------------------------------------------------------------------ */
/* Cleanup helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * DESTROY_TEX / FREE_CHUNK — null-safe one-liner resource release.
 *
 * Both macros check for NULL before destroying, then set the pointer to
 * NULL so accidental double-frees become safe no-ops.  They are intended
 * for use inside game_cleanup() where ~35 identical if-guard-destroy-null
 * blocks would otherwise appear.
 *
 * Slot owners clear pointers after unloading; borrowers never call these.
 */
#define DESTROY_TEX(tex) \
    do { texture_unload(tex); (tex) = NULL; } while (0)

#define FREE_CHUNK(snd) \
    do { sound_unload(snd); (snd) = NULL; } while (0)

/* ------------------------------------------------------------------ */
/* GameState — the single source of truth for everything the game owns */
/* ------------------------------------------------------------------ */

/*
 * Camera — tracks the horizontal scroll position of the viewport.
 *
 * cam.x is the left edge of the visible window in world coordinates.
 * To convert a world-space x to a screen-space x: screen_x = world_x - cam.x
 * The camera lerps toward a target that keeps the player centered, with a
 * directional lookahead offset. It clamps at the world boundaries so the
 * black void beyond WORLD_W is never shown.
 */
typedef struct {
    float x;   /* left edge of the visible window in world-space logical pixels */
} GameCamera;

typedef struct {
    const void *current_level;  /* pointer to the active LevelDef          */
    int         fog_enabled;    /* 1 = fog rendering active, 0 = disabled */
    int         water_enabled;  /* 1 = water strip rendered, 0 = disabled */
    int         world_w;        /* level width in pixels                  */
} LevelRuntime;

typedef struct {
    int score_per_life;  /* score threshold for bonus life      */
    int coin_score;      /* points awarded per coin collected   */
} GameRules;

/*
 * GameLoopState — bookkeeping for the fixed-step frame loop (game_timing.c).
 *
 * Real time between frames is added to `accumulator`; the simulation then
 * consumes it in steps of exactly 1/TARGET_FPS seconds. Leftover time (less
 * than one step) waits for the next frame.
 */
typedef struct {
    double prev_time;      /* GetTime() seconds when the previous frame began   */
    double accumulator;    /* real seconds waiting to be simulated              */
    int    clock_started;  /* 0 = no previous frame time yet (first frame)      */
    int    fp_prev_riding; /* float platform player stood on last step          */
    int    smoke_frames_run; /* frames actually executed by a smoke run         */
} GameLoopState;

/*
 * GameRoute — requests leaving the current GameState.
 *
 * GameState owns gameplay only.  AppSession consumes these requests after the
 * frame has rendered and performs every cross-screen transition.
 */
typedef enum {
    GAME_ROUTE_NONE = 0,
    GAME_ROUTE_NEXT_LEVEL,
    GAME_ROUTE_REPLAY,
    GAME_ROUTE_LEVEL_SELECT,
    GAME_ROUTE_EXIT,
    GAME_ROUTE_FATAL,
    GAME_ROUTE_SMOKE_EXIT
} GameRoute;

typedef struct {
    int   complete;           /* 1 = last star collected; show overlay     */
    float level_elapsed;      /* active level timer in seconds             */
    int   level_coin_total;   /* coins present when current level loaded   */
    int   coins_collected;    /* coins collected at completion summary     */
    int   coin_total;         /* total coins shown at completion summary   */
    float elapsed;            /* elapsed seconds shown at summary          */
    int   pending_next_phase; /* Enter/Start loads next phase              */
    int   next_phase_failed;  /* 1 = Next Level load failed: hide it, say so */
    char  next_phase[256];    /* next TOML path shown/loaded               */
} GameCompletionState;

typedef struct {
    Texture2D *floor_tile;
    Texture2D *platform;
    Texture2D *spider;
    Texture2D *jumping_spider;
    Texture2D *bird;
    Texture2D *faster_bird;
    Texture2D *fish;
    Texture2D *faster_fish;
    Texture2D *coin;
    Texture2D *vine_green;
    Texture2D *vine_brown;
    Texture2D *ladder;
    Texture2D *rope;
    Texture2D *bouncepad_medium;
    Texture2D *bouncepad_small;
    Texture2D *bouncepad_high;
    Texture2D *rail;
    Texture2D *spike_block;
    Texture2D *float_platform;
    Texture2D *bridge;
    Texture2D *star_yellow;
    Texture2D *star_green;
    Texture2D *star_red;
    Texture2D *last_star;
    Texture2D *axe_trap;
    Texture2D *circular_saw;
    Texture2D *blue_flame;
    Texture2D *fire_flame;
    Texture2D *spike;
    Texture2D *spike_platform;
} TextureResources;

/*
 * PlatformTileCache — one texture per distinct platform tile image.
 *
 * Many pillars in a level share one tileset (02_lugio_02 draws 23 of them
 * from stone_platform.png). Decoding that PNG once per pillar wasted load
 * time and GPU memory, so level_loader.c keeps one entry per path: the cache
 * owns each texture and every Platform::tex naming that path borrows it.
 * An entry whose image failed to load keeps texture == NULL, so the warning
 * prints once and the file is not read again.
 */
#define PLATFORM_TILE_PATH_SIZE 64  /* same capacity as PlatformPlacement::tile_path */
typedef struct {
    char       path[PLATFORM_TILE_PATH_SIZE];
    Texture2D *texture;   /* owned GPU texture; NULL when loading failed */
    int        in_use;    /* scratch mark while a new level is applied   */
} PlatformTile;

typedef struct {
    PlatformTile tiles[MAX_PLATFORMS]; /* at most one per platform */
    int          count;
} PlatformTileCache;

typedef struct {
    SoundEffect *jump;
    SoundEffect *coin;
    SoundEffect *hit;
    SoundEffect *spring;
    SoundEffect *axe;
    SoundEffect *flap;
    SoundEffect *spider_attack;
    SoundEffect *dive;
    MusicTrack *music;
} AudioResources;

typedef struct {
    RenderTexture2D frame_target; /* owned logical canvas; session owns window */
    int controller;               /* raylib device index + 1; zero means none */
    TextureResources textures;    /* owned GPU textures */
    AudioResources audio;         /* owned samples and music stream */
    ParallaxSystem      parallax;  /* multi-layer scrolling background            */
    Player        player;      /* the player, stored by value (not a pointer) */
    Platform      platforms[MAX_PLATFORMS]; /* one-way pillar definitions     */
    int           platform_count;           /* how many platforms are active  */
    PlatformTileCache platform_tiles;       /* owns the textures platforms borrow */
    Water         water;        /* animated water strip at the bottom of screen*/
    FogSystem     fog;         /* atmospheric fog overlay — topmost layer      */
    Spider        spiders[MAX_SPIDERS]; /* ground-patrol enemy instances      */
    int           spider_count;         /* number of active spiders           */
    JumpingSpider jumping_spiders[MAX_JUMPING_SPIDERS]; /* jump-patrol enemies*/
    int           jumping_spider_count; /* number of active jumping spiders   */
    Bird          birds[MAX_BIRDS]; /* slow sine-wave sky patrol enemies      */
    int           bird_count;       /* number of active birds                 */
    FasterBird    faster_birds[MAX_FASTER_BIRDS]; /* fast sky patrol enemies  */
    int           faster_bird_count; /* number of active faster birds         */
    Fish          fish[MAX_FISH]; /* jumping water enemy instances             */
    int           fish_count;      /* number of active fish                     */
    Coin          coins[MAX_COINS]; /* collectible coin instances             */
    int           coin_count;       /* number of coins placed                */
    VineDecor     vines[MAX_VINES]; /* static scenery vine instances               */
    int           vine_count;       /* number of vine decorations placed           */
    LadderDecor   ladders[MAX_LADDERS]; /* climbable ladder instances             */
    int           ladder_count;    /* number of ladders placed                    */
    RopeDecor     ropes[MAX_ROPES];/* climbable rope instances                    */
    int           rope_count;      /* number of ropes placed                      */
    Bouncepad     bouncepads_medium[MAX_BOUNCEPADS_MEDIUM]; /* wood pads             */
    int           bouncepad_medium_count;     /* number of medium bouncepads         */
    Bouncepad     bouncepads_small[MAX_BOUNCEPADS_SMALL];   /* green pads            */
    int           bouncepad_small_count;      /* number of small bouncepads          */
    Bouncepad     bouncepads_high[MAX_BOUNCEPADS_HIGH];     /* red pads              */
    int           bouncepad_high_count;       /* number of high bouncepads           */
    Rail          rails[MAX_RAILS];/* level rail loop definitions                 */
    int           rail_count;      /* number of active rail loops                 */
    SpikeBlock    spike_blocks[MAX_SPIKE_BLOCKS]; /* rail-riding hazard instances */
    int           spike_block_count;              /* number of active blocks      */
    FloatPlatform  float_platforms[MAX_FLOAT_PLATFORMS];    /* hovering surface instances        */
    int            float_platform_count;                    /* number of float platforms placed  */
    Bridge        bridges[MAX_BRIDGES];/* tiled crumble walkway instances      */
    int           bridge_count;        /* number of active bridges             */
    int           floor_gaps[MAX_FLOOR_GAPS]; /* left-edge x of each floor gap     */
    int           floor_gap_count;           /* number of active floor gaps       */
    HealthStar    star_yellows[MAX_STAR_YELLOWS]; /* yellow health stars           */
    int           star_yellow_count;     /* number of star yellows placed       */
    HealthStar    star_greens[MAX_STAR_GREENS];   /* green health stars            */
    int           star_green_count;      /* number of star greens placed        */
    HealthStar    star_reds[MAX_STAR_REDS];       /* red health stars              */
    int           star_red_count;        /* number of star reds placed          */
    AxeTrap       axe_traps[MAX_AXE_TRAPS]; /* swinging/spinning axe hazards  */
    int           axe_trap_count;        /* number of axe traps placed         */
    CircularSaw   circular_saws[MAX_CIRCULAR_SAWS]; /* fast patrol saw hazards */
    int           circular_saw_count;    /* number of circular saws placed     */
    BlueFlame     blue_flames[MAX_BLUE_FLAMES]; /* erupting fire hazards from gaps */
    int           blue_flame_count;     /* number of blue flames placed        */
    BlueFlame     fire_flames[MAX_FIRE_FLAMES]; /* erupting fire hazards (fire variant) */
    int           fire_flame_count;     /* number of fire flames placed        */
    FasterFish    faster_fish[MAX_FASTER_FISH]; /* fast jumping fish enemies   */
    int           faster_fish_count;     /* number of faster fish placed       */
    LastStar      last_star;             /* end-of-level collectible           */
    SpikeRow      spike_rows[MAX_SPIKE_ROWS]; /* static ground spike hazards  */
    int           spike_row_count;       /* number of spike rows placed        */
    SpikePlatform spike_platforms[MAX_SPIKE_PLATFORMS]; /* elevated spike surfs*/
    int           spike_platform_count;  /* number of spike platforms placed   */
    Hud           hud;         /* HUD display: hearts, lives, score           */
    int           hearts;      /* current hit points (0–MAX_HEARTS)           */
    int           lives;       /* remaining lives; <0 triggers game over      */
    int           score;       /* cumulative score from collecting coins      */
    int           score_life_next; /* next bonus threshold; 0 = score ceiling reached */
    GameCamera    camera;      /* viewport scroll position; updated every frame*/
    int           running;     /* active game frame flag; session owns routes  */
    GameRoute     route;       /* explicit request consumed by AppSession      */
    int           game_over;   /* 1 = game-over overlay awaiting restart      */
    int           paused;      /* 1 = pause overlay active; physics/music frozen */
    unsigned int  pause_reasons; /* bitmask of active pause reasons             */
    float         respawn_x;      /* resolved respawn placement x               */
    float         respawn_y;      /* resolved respawn placement y               */
    int           checkpoint_index; /* authored checkpoint index, -1 before one */
    CheckpointFeedbackKind checkpoint_feedback_kind; /* explicit HUD cue reason */
    uint32_t      checkpoint_feedback_until; /* cue expiry, game_checkpoint_clock_ms time */
    double        sim_time;    /* seconds simulated so far; only fixed steps
                                  advance it, so a pause stops it            */
    int           legacy_checkpoint_screen; /* last automatic screen boundary   */
    int           debug_mode;  /* 1 = debug overlays active (--debug flag)   */
    int           smoke_test_frames; /* >0 = exit after this many frames     */
    char          replay_script_path[256]; /* optional replay script name     */
    char          replay_dir[256]; /* folder holding replay scripts; "" = default */
    unsigned int  replay_input_mask; /* replay keys active for this frame    */
    unsigned int  replay_held_mask;  /* replay keys held across frames       */
    int           replay_frame; /* current deterministic replay frame     */
    struct GameReplayEvent *replay_events; /* owned parsed replay, NULL in normal play */
    int           replay_event_count;
    int           replay_cursor;
    char          level_path[GAME_LEVEL_PATH_MAX]; /* TOML level to load (--level flag) */
    void         *level_def;   /* owned active LevelDef backing storage   */
    DebugOverlay  debug;       /* FPS counter, collision vis, event log      */

    LevelRuntime runtime; /* active LevelDef pointer, level width, effect flags */
    GameRules    rules;   /* score/life and collectible rule values             */
    GameLoopState loop;   /* frame loop scratch state for native/WASM loops     */
    GameCompletionState completion; /* timer, summary, and next-phase state   */
    int           terminal_action_index; /* focused terminal action row        */
    unsigned int  input_release_keyboard_mask;   /* keys held across a route    */
    unsigned int  input_release_controller_mask; /* buttons held across a route */
    int           input_release_latched;         /* physical input gate active   */
    struct GameProfile *profile; /* borrowed from the owning AppSession */
    struct SettingsMenu *settings_menu; /* borrowed; screen cleanup releases its textures */
    char profile_level_key[256];
    int profile_completion_recorded;
    int resumed;        /* 1 = this run started from a saved Continue point */
    int level_score_start;
    unsigned int random_seed;
    uint64_t source_level_hash; /* source bytes corresponding to active LevelDef */
    struct {
        int frozen, step_requested, slow_mode, physics_field, entity_index;
        int show_keys;                    /* F5 toggles the key help panel   */
        uint64_t tuning_visible_until;    /* clock_millis() deadline for the
                                             tuning line after F6/F7/-/+    */
    } inspector;
    struct GameExperiment *experiment; /* owned opt-in capture/replay */
    struct GameGhost *ghost; /* owned time-trial recorder and best run; NULL without a profile */
} GameState;

/* ------------------------------------------------------------------ */
/* Function declarations                                               */
/* These tell the compiler "these functions exist". Any file that       */
/* includes this header can call them.                                  */
/* ------------------------------------------------------------------ */

/* Create the logical render target and load screen-owned resources. */
int game_init(GameState *gs);

/* Execute one game frame. Returns 1 only after presentation. */
int game_frame(GameState *gs);

/* Free every resource owned by the game in reverse-init order. */
void game_cleanup(GameState *gs);

/* Load the next phase/level when last_star is collected with next_phase set. */
int game_load_next_phase(GameState *gs);

/* Snapshot end-of-level stats and show the completion summary overlay. */
void game_complete_level(GameState *gs);
