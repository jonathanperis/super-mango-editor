/*
 * player.h — Public interface for the player module.
 *
 * Defines the Player data structure and declares the five functions
 * that manage a player's full lifecycle: init, input, update, render, cleanup.
 */
#pragma once

#include <SDL.h>        /* SDL_Texture, SDL_Renderer, SDL_Rect */
#include <SDL_mixer.h>  /* Mix_Chunk */
#include "platform.h"   /* Platform, MAX_PLATFORMS — needed for player_update signature */

/*
 * AnimState — which animation sequence is currently playing.
 * The value maps directly to the sheet row (see ANIM_ROW[] in player.c).
 */
typedef enum {
    ANIM_IDLE = 0,  /* standing still:       sheet row 0, 4 frames */
    ANIM_WALK,      /* running left/right:   sheet row 1, 4 frames */
    ANIM_JUMP,      /* rising after a jump:  sheet row 2, 2 frames */
    ANIM_FALL       /* falling under gravity: sheet row 3, 1 frame */
} AnimState;

/*
 * Player — all the data needed to represent the player character.
 *
 * Position and velocity use float (not int) so that sub-pixel movement
 * accumulated via delta time doesn't get truncated each frame.
 * We only convert to int at the very last moment when drawing.
 */
typedef struct {
    float x;            /* horizontal position in pixels (top-left corner)  */
    float y;            /* vertical   position in pixels (top-left corner)   */
    float vx;           /* horizontal velocity: pixels per second            */
    float vy;           /* vertical   velocity: pixels per second (gravity)  */
    float speed;        /* horizontal max speed in pixels per second         */
    int   w;            /* display width  of one frame in pixels (48)        */
    int   h;            /* display height of one frame in pixels (48)        */
    int   on_ground;    /* 1 if standing on the floor, 0 if airborne         */
    AnimState anim_state;       /* which animation is active (idle/walk/jump/fall) */
    int       anim_frame_index; /* current frame index within the animation       */
    Uint32    anim_timer_ms;    /* ms accumulated in the current frame            */
    int       facing_left;      /* 1 = mirror sprite horizontally                 */
    float     hurt_timer;        /* seconds remaining of invincibility blink; 0 = normal */
    SDL_Rect     frame;   /* source rect: which part of the sheet to draw    */
    SDL_Texture *texture; /* GPU image handle; NULL until player_init runs   */
} Player;

/* Load the player texture and set its initial position. */
void player_init(Player *player, SDL_Renderer *renderer);

/* Sample the keyboard every frame and set vx/vy accordingly. */
void player_handle_input(Player *player, Mix_Chunk *snd_jump);

/* Move the player by velocity × dt; resolve floor and one-way platform collisions. */
void player_update(Player *player, float dt, const Platform *platforms, int platform_count);

/* Draw the player sprite at its current position, offset by the camera. */
void player_render(Player *player, SDL_Renderer *renderer, int cam_x);

/* Return the player's tightly-inset physics hitbox (logical pixels). */
SDL_Rect player_get_hitbox(const Player *player);

/* Reset the player's position and state without reloading the texture. */
void player_reset(Player *player);

/* Release the player's GPU texture. */
void player_cleanup(Player *player);
