/*
 * game.h — Public interface for the game module.
 *
 * Defines:
 *   - Compile-time constants shared across all files.
 *   - The GameState struct that holds everything the game needs.
 *   - Declarations for the three functions that drive the game.
 */

/*
 * #pragma once is a non-standard but universally supported header guard.
 * It tells the compiler: "include this file only once per translation unit,
 * even if #included multiple times". Prevents duplicate-definition errors.
 */
#pragma once

#include <SDL.h>        /* SDL_Window, SDL_Renderer, SDL_Texture */
#include <SDL_mixer.h>  /* Mix_Chunk */
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
#include "collectibles/star_yellow.h"/* StarYellow — health-restoring collectible */
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

#define WINDOW_TITLE  "Super Mango"   /* title bar text                */
#define WINDOW_W      800             /* OS window width  in pixels    */
#define WINDOW_H      600             /* OS window height in pixels    */
#define TARGET_FPS    60             /* desired frames per second      */

/*
 * GAME_W / GAME_H — the internal (logical) rendering resolution.
 *
 * All game objects are positioned and sized in this coordinate space.
 * SDL automatically scales this canvas up to fill the OS window, giving
 * a 2× pixel scale (800/400 = 2, 600/300 = 2). This makes every sprite
 * and tile appear twice as large on screen without changing any game logic.
 */
#define GAME_W        400
#define GAME_H        300

/*
 * TILE_SIZE — display size of one grass tile in pixels.
 * The Grass_Tileset.png is 48×48; we render it at its natural size.
 */
#define TILE_SIZE     48

/*
 * FLOOR_Y — the Y coordinate of the top edge of the floor.
 * Anything at or below this Y is "inside" the floor.
 * Uses GAME_H because all positions live in logical (400×300) space.
 */
#define FLOOR_Y       (GAME_H - TILE_SIZE)

/*
 * GRAVITY — downward acceleration in pixels per second squared.
 * Applied every frame so the player accelerates toward the floor.
 */
#define GRAVITY       800.0f

/*
 * WORLD_W — total logical width of the level in pixels.
 * The visible window is still GAME_W (400 px); the camera scrolls to reveal
 * the rest. WORLD_W = 4 × GAME_W gives four screens of horizontal space.
 */
#define WORLD_W       1600

/*
 * FLOOR_GAP_W — width of each floor gap in logical pixels.
 * MAX_FLOOR_GAPS — maximum number of gaps the level can hold.
 *
 * Floor gaps are holes in the ground floor that expose the water below.
 * Falling into any gap costs a life (instant death, not a hurt point).
 * Each gap is defined by its left-edge x coordinate; all are FLOOR_GAP_W wide.
 */
#define FLOOR_GAP_W         32
#define MAX_FLOOR_GAPS      16

/*
 * CAM_LOOKAHEAD_VX_FACTOR — how many pixels of lookahead per px/s of player
 * horizontal velocity.  The lookahead scales continuously with vx: at rest
 * it is exactly 0 (player centred), at full run speed it peaks near
 * CAM_LOOKAHEAD_MAX.  The camera therefore reveals more terrain the faster
 * the player is moving, and smoothly recentres when they stop.
 *
 * Example: factor 0.20 × 220 px/s (run max) = 44 px of lookahead.
 */
#define CAM_LOOKAHEAD_VX_FACTOR  0.20f

/*
 * CAM_LOOKAHEAD_MAX — hard cap (pixels) on the lookahead in either direction.
 * Prevents the offset from growing excessively if vx ever exceeds normal max.
 */
#define CAM_LOOKAHEAD_MAX  50.0f

/*
 * CAM_SMOOTHING — lerp speed factor (dimensionless, applied per second).
 * Each frame the camera closes (CAM_SMOOTHING × dt) of the remaining gap to
 * the target. 8.0 gives responsive follow without snapping. Lower = laggier.
 */
#define CAM_SMOOTHING  8.0f

/*
 * CAM_SNAP_THRESHOLD — when the remaining gap between cam_x and its target
 * is smaller than this many pixels, snap exactly instead of lerping.
 * Prevents endless sub-pixel micro-drift each frame.
 */
#define CAM_SNAP_THRESHOLD  0.5f

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
 * SDL_DestroyTexture(NULL) and Mix_FreeChunk(NULL) are SDL-defined safe
 * no-ops, so the guard is a belt-and-suspenders layer that also clears
 * the pointer to prevent the same resource from being freed twice.
 */
#define DESTROY_TEX(tex) \
    do { if (tex) { SDL_DestroyTexture(tex); (tex) = NULL; } } while (0)

#define FREE_CHUNK(snd) \
    do { if (snd) { Mix_FreeChunk(snd); (snd) = NULL; } } while (0)

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
} Camera;

typedef struct {
    SDL_Window         *window;     /* the OS window (created by SDL)              */
    SDL_Renderer       *renderer;  /* GPU-accelerated 2D drawing context          */
    SDL_GameController *controller;  /* first connected gamepad; NULL = none          */
    /*
     * Gamepad init state machine (avoids blocking the main thread):
     *   0 = idle / done
     *   1 = first frame rendered — start background thread next
     *   2 = thread running — show HUD message, check each frame
     */
    int         ctrl_pending_init;
    SDL_Thread *ctrl_init_thread;    /* background thread for SDL_InitSubSystem call   */
    volatile int ctrl_init_done;     /* set to 1 by thread when subsystem is ready     */
    SDL_Texture *ctrl_init_msg_tex;  /* cached HUD text for the init message; NULL when not shown */
    ParallaxSystem      parallax;  /* multi-layer scrolling background            */
    SDL_Texture   *floor_tile; /* grass tile repeated across the floor layer  */
    SDL_Texture  *platform_tex;/* shared tile texture for all pillars         */
    SDL_Texture  *spider_tex;  /* shared texture for all spider enemies       */
    Mix_Chunk    *snd_jump;    /* WAV chunk for the jump sound effect         */
    Mix_Chunk    *snd_coin;    /* WAV chunk played when collecting a coin     */
    Mix_Chunk    *snd_hit;     /* WAV chunk played when player gets hurt      */
    Mix_Music    *music;       /* looping background sound (WAV/OGG)          */
    Player        player;      /* the player, stored by value (not a pointer) */
    Platform      platforms[MAX_PLATFORMS]; /* one-way pillar definitions     */
    int           platform_count;           /* how many platforms are active  */
    Water         water;        /* animated water strip at the bottom of screen*/
    FogSystem     fog;         /* atmospheric fog overlay — topmost layer      */
    Spider        spiders[MAX_SPIDERS]; /* ground-patrol enemy instances      */
    int           spider_count;         /* number of active spiders           */
    SDL_Texture  *jumping_spider_tex;  /* texture for jumping spider enemies  */
    JumpingSpider jumping_spiders[MAX_JUMPING_SPIDERS]; /* jump-patrol enemies*/
    int           jumping_spider_count; /* number of active jumping spiders   */
    SDL_Texture  *bird_tex;    /* shared texture for Bird enemies (Bird_2.png) */
    Bird          birds[MAX_BIRDS]; /* slow sine-wave sky patrol enemies      */
    int           bird_count;       /* number of active birds                 */
    SDL_Texture  *faster_bird_tex;  /* texture for FasterBird (Bird_1.png)    */
    FasterBird    faster_birds[MAX_FASTER_BIRDS]; /* fast sky patrol enemies  */
    int           faster_bird_count; /* number of active faster birds         */
    SDL_Texture  *fish_tex;    /* shared texture for all fish enemies          */
    Fish          fish[MAX_FISH]; /* jumping water enemy instances             */
    int           fish_count;      /* number of active fish                     */
    SDL_Texture  *coin_tex;    /* shared texture for all coin collectibles    */
    Coin          coins[MAX_COINS]; /* collectible coin instances             */
    int           coin_count;       /* number of coins placed                */
    SDL_Texture  *vine_green_tex;  /* lush vine (forest/fertile themes)       */
    SDL_Texture  *vine_brown_tex;  /* dried vine (arid/volcanic themes)       */
    VineDecor     vines[MAX_VINES]; /* static scenery vine instances               */
    int           vine_count;       /* number of vine decorations placed           */
    SDL_Texture  *ladder_tex;      /* shared texture for ladder climbables        */
    LadderDecor   ladders[MAX_LADDERS]; /* climbable ladder instances             */
    int           ladder_count;    /* number of ladders placed                    */
    SDL_Texture  *rope_tex;        /* shared texture for rope climbables          */
    RopeDecor     ropes[MAX_ROPES];/* climbable rope instances                    */
    int           rope_count;      /* number of ropes placed                      */
    SDL_Texture  *bouncepad_medium_tex;        /* wood (medium) bouncepad texture      */
    Bouncepad     bouncepads_medium[MAX_BOUNCEPADS_MEDIUM]; /* wood pads             */
    int           bouncepad_medium_count;     /* number of medium bouncepads         */
    SDL_Texture  *bouncepad_small_tex;        /* green (small) bouncepad texture      */
    Bouncepad     bouncepads_small[MAX_BOUNCEPADS_SMALL];   /* green pads            */
    int           bouncepad_small_count;      /* number of small bouncepads          */
    SDL_Texture  *bouncepad_high_tex;         /* red (high) bouncepad texture         */
    Bouncepad     bouncepads_high[MAX_BOUNCEPADS_HIGH];     /* red pads              */
    int           bouncepad_high_count;       /* number of high bouncepads           */
    Mix_Chunk    *snd_spring;  /* WAV chunk played when bouncepad is triggered    */
    SDL_Texture  *rail_tex;        /* shared texture for all rail tiles           */
    Rail          rails[MAX_RAILS];/* level rail loop definitions                 */
    int           rail_count;      /* number of active rail loops                 */
    SDL_Texture  *spike_block_tex; /* shared texture for all spike block entities */
    SpikeBlock    spike_blocks[MAX_SPIKE_BLOCKS]; /* rail-riding hazard instances */
    int           spike_block_count;              /* number of active blocks      */
    SDL_Texture  *float_platform_tex;                       /* Platform.png — 3-slice strip      */
    FloatPlatform  float_platforms[MAX_FLOAT_PLATFORMS];    /* hovering surface instances        */
    int            float_platform_count;                    /* number of float platforms placed  */
    SDL_Texture  *bridge_tex;          /* shared texture for bridge tiles       */
    Bridge        bridges[MAX_BRIDGES];/* tiled crumble walkway instances      */
    int           bridge_count;        /* number of active bridges             */
    int           floor_gaps[MAX_FLOOR_GAPS]; /* left-edge x of each floor gap     */
    int           floor_gap_count;           /* number of active floor gaps       */
    SDL_Texture  *star_yellow_tex;       /* shared texture for star yellow pickups*/
    SDL_Texture  *star_green_tex;       /* shared texture for star green pickups */
    SDL_Texture  *star_red_tex;         /* shared texture for star red pickups   */
    SDL_Texture  *last_star_tex;         /* dedicated texture for end-of-level star*/
    StarYellow    star_yellows[MAX_STAR_YELLOWS]; /* health-restoring collectibles */
    int           star_yellow_count;     /* number of star yellows placed       */
    StarYellow    star_greens[MAX_STAR_YELLOWS]; /* green health-restoring collectibles */
    int           star_green_count;      /* number of star greens placed        */
    StarYellow    star_reds[MAX_STAR_YELLOWS]; /* red health-restoring collectibles */
    int           star_red_count;        /* number of star reds placed          */
    SDL_Texture  *axe_trap_tex;          /* shared texture for axe trap hazards*/
    AxeTrap       axe_traps[MAX_AXE_TRAPS]; /* swinging/spinning axe hazards  */
    int           axe_trap_count;        /* number of axe traps placed         */
    Mix_Chunk    *snd_axe;               /* SFX played on axe swing completion */
    Mix_Chunk    *snd_flap;              /* SFX for bird wing flap             */
    Mix_Chunk    *snd_spider_attack;     /* SFX for jumping spider leap        */
    Mix_Chunk    *snd_dive;              /* SFX for falling into sea gap       */
    SDL_Texture  *circular_saw_tex;      /* shared texture for circular saw hazards*/
    CircularSaw   circular_saws[MAX_CIRCULAR_SAWS]; /* fast patrol saw hazards */
    int           circular_saw_count;    /* number of circular saws placed     */
    SDL_Texture  *blue_flame_tex;        /* shared texture for blue flame hazards */
    BlueFlame     blue_flames[MAX_BLUE_FLAMES]; /* erupting fire hazards from gaps */
    int           blue_flame_count;     /* number of blue flames placed        */
    SDL_Texture  *fire_flame_tex;        /* shared texture for fire flame hazards */
    BlueFlame     fire_flames[MAX_BLUE_FLAMES]; /* erupting fire hazards (fire variant) */
    int           fire_flame_count;     /* number of fire flames placed        */
    SDL_Texture  *faster_fish_tex;       /* shared texture for faster fish     */
    FasterFish    faster_fish[MAX_FASTER_FISH]; /* fast jumping fish enemies   */
    int           faster_fish_count;     /* number of faster fish placed       */
    LastStar      last_star;             /* end-of-level collectible           */
    SDL_Texture  *spike_tex;             /* shared texture for ground spikes   */
    SpikeRow      spike_rows[MAX_SPIKE_ROWS]; /* static ground spike hazards  */
    int           spike_row_count;       /* number of spike rows placed        */
    SDL_Texture  *spike_platform_tex;    /* shared texture for spike platforms */
    SpikePlatform spike_platforms[MAX_SPIKE_PLATFORMS]; /* elevated spike surfs*/
    int           spike_platform_count;  /* number of spike platforms placed   */
    Hud           hud;         /* HUD display: hearts, lives, score           */
    int           hearts;      /* current hit points (0–MAX_HEARTS)           */
    int           lives;       /* remaining lives; <0 triggers game over      */
    int           score;       /* cumulative score from collecting coins      */
    int           score_life_next; /* score threshold for next bonus life     */
    Camera        camera;      /* viewport scroll position; updated every frame*/
    int           running;     /* loop flag: 1 = keep running, 0 = quit       */
    int           paused;      /* 1 = window lost focus; physics/music frozen */
    int           debug_mode;  /* 1 = debug overlays active (--debug flag)   */
    char          level_path[256]; /* JSON level to load (--level flag)      */
    DebugOverlay  debug;       /* FPS counter, collision vis, event log      */

    /* ---- Level-wide configuration (set by level_load from LevelDef) -- */
    const void   *current_level;    /* pointer to the active LevelDef          */
    int           fog_enabled;      /* 1 = fog rendering active, 0 = disabled  */
    int           water_enabled;    /* 1 = water strip rendered, 0 = disabled  */
    int           world_w;         /* level width in pixels (screen_count * GAME_W) */
    int           score_per_life;   /* score threshold for bonus life           */
    int           coin_score;       /* points awarded per coin collected        */

    /* ---- Loop state (persists across frames for emscripten callback) - */
    Uint64        loop_prev_ticks;  /* timestamp of previous frame         */
    int           fp_prev_riding;   /* float platform player stood on last frame */
} GameState;

/* ------------------------------------------------------------------ */
/* Function declarations                                               */
/* These tell the compiler "these functions exist; their bodies are    */
/* in game.c". Any file that includes this header can call them.       */
/* ------------------------------------------------------------------ */

/* Create the window, renderer, and load all textures. */
void game_init(GameState *gs);

/* Run the main game loop until gs->running becomes 0. */
void game_loop(GameState *gs);

/* Free every resource owned by the game in reverse-init order. */
void game_cleanup(GameState *gs);
