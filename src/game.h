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
#include "coin.h"       /* Coin struct + MAX_COINS constant                  */
#include "hud.h"        /* Hud struct — HUD display resources                */
#include "parallax.h"   /* ParallaxSystem — multi-layer scrolling background */

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
    SDL_Window    *window;      /* the OS window (created by SDL)              */
    SDL_Renderer  *renderer;   /* GPU-accelerated 2D drawing context          */
    ParallaxSystem parallax;   /* multi-layer scrolling background            */
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
    SDL_Texture  *coin_tex;    /* shared texture for all coin collectibles    */
    Coin          coins[MAX_COINS]; /* collectible coin instances             */
    int           coin_count;       /* number of coins placed                */
    Hud           hud;         /* HUD display: hearts, lives, score           */
    int           hearts;      /* current hit points (0–MAX_HEARTS)           */
    int           lives;       /* remaining lives; 0 triggers game over       */
    int           score;       /* cumulative score from collecting coins      */
    int           coins_for_heart; /* coins collected toward next heart restore */
    Camera        camera;      /* viewport scroll position; updated every frame*/
    int           running;     /* loop flag: 1 = keep running, 0 = quit       */
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
