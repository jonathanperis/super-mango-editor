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
#include "player.h"     /* Player struct — embedded by value in GameState */
#include "platform.h"   /* Platform struct + MAX_PLATFORMS constant */
#include "water.h"      /* Water struct — animated bottom strip              */
#include "fog.h"        /* FogSystem struct — atmospheric fog overlay      */
#include "spider.h"     /* Spider struct + MAX_SPIDERS constant              */
#include "fish.h"       /* Fish struct + MAX_FISH constant                  */
#include "coin.h"       /* Coin struct + MAX_COINS constant                  */
#include "vine.h"       /* VineDecor struct + MAX_VINES constant              */
#include "bouncepad.h"       /* Bouncepad struct — shared mechanics            */
#include "bouncepad_small.h" /* Green bouncepad (small jump) placement        */
#include "bouncepad_medium.h"/* Wood bouncepad (medium jump) placement        */
#include "bouncepad_high.h"  /* Red bouncepad (high jump) placement           */
#include "hud.h"        /* Hud struct — HUD display resources                */
#include "parallax.h"   /* ParallaxSystem — multi-layer scrolling background */
#include "rail.h"           /* Rail, RailTile — rail path system              */
#include "spike_block.h"    /* SpikeBlock — rail-riding hazard entity          */
#include "float_platform.h" /* FloatPlatform — hovering/crumble/rail surfaces */
#include "bridge.h"         /* Bridge — tiled crumble walkway                */
#include "jumping_spider.h" /* JumpingSpider — jumping patrol enemy          */
#include "bird.h"           /* Bird — slow sine-wave sky patrol             */
#include "faster_bird.h"    /* FasterBird — fast sine-wave sky patrol       */
#include "yellow_star.h"    /* YellowStar — health-restoring collectible    */
#include "axe_trap.h"       /* AxeTrap — swinging/spinning axe hazard       */
#include "circular_saw.h"  /* CircularSaw — fast rotating patrol hazard    */
#include "flame.h"         /* Flame — erupting fire hazard from sea gaps   */
#include "ladder.h"        /* LadderDecor — climbable ladder                */
#include "rope.h"          /* RopeDecor — climbable rope                   */
#include "faster_fish.h"   /* FasterFish — fast variant of jumping fish     */
#include "last_star.h"     /* LastStar — end-of-level collectible           */
#include "spike.h"         /* SpikeRow — static ground spike hazards       */
#include "spike_platform.h"/* SpikePlatform — elevated spike hazard surface*/
#include "debug.h"        /* DebugOverlay — debug collision/FPS/log overlay  */

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
 * SEA_GAP_W — width of each sea gap in logical pixels.
 * MAX_SEA_GAPS — maximum number of gaps the level can hold.
 *
 * Sea gaps are holes in the ground floor that expose the water below.
 * Falling into any gap costs a life (instant death, not a hurt point).
 * Each gap is defined by its left-edge x coordinate; all are SEA_GAP_W wide.
 */
#define SEA_GAP_W         32
#define MAX_SEA_GAPS       8

/*
 * CAM_LOOKAHEAD — extra pixels the camera shifts in the direction the player
 * faces, revealing more terrain ahead before the player reaches the edge.
 * Increase this value for more forward visibility; set to 0 to disable.
 */
#define CAM_LOOKAHEAD  40

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
    SDL_GameController *controller;/* first connected gamepad; NULL = none         */
    ParallaxSystem      parallax;  /* multi-layer scrolling background            */
    SDL_Texture   *floor_tile; /* grass tile repeated across the floor layer  */
    SDL_Texture  *platform_tex;/* shared tile texture for all pillars         */
    SDL_Texture  *spider_tex;  /* shared texture for all spider enemies       */
    Mix_Chunk    *snd_jump;    /* WAV chunk for the jump sound effect         */
    Mix_Chunk    *snd_coin;    /* WAV chunk played when collecting a coin     */
    Mix_Chunk    *snd_hit;     /* WAV chunk played when player gets hurt      */
    Mix_Music    *music;       /* MP3 stream for the looping background music */
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
    SDL_Texture  *vine_tex;    /* shared texture for all vine decorations         */
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
    int           sea_gaps[MAX_SEA_GAPS]; /* left-edge x of each sea gap       */
    int           sea_gap_count;         /* number of active sea gaps          */
    SDL_Texture  *yellow_star_tex;       /* shared texture for yellow star pickups*/
    YellowStar    yellow_stars[MAX_YELLOW_STARS]; /* health-restoring collectibles */
    int           yellow_star_count;     /* number of yellow stars placed       */
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
    SDL_Texture  *flame_tex;             /* shared texture for flame hazards   */
    Flame         flames[MAX_FLAMES];    /* erupting fire hazards from gaps    */
    int           flame_count;           /* number of flames placed            */
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
    DebugOverlay  debug;       /* FPS counter, collision vis, event log      */
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
