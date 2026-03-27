/*
 * player.h — Public interface for the player module.
 *
 * Defines the Player data structure and declares the five functions
 * that manage a player's full lifecycle: init, input, update, render, cleanup.
 */
#pragma once

#include <SDL.h>  /* SDL_Texture, SDL_Renderer */

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
    SDL_Rect     frame;   /* source rect: which part of the sheet to draw    */
    SDL_Texture *texture; /* GPU image handle; NULL until player_init runs   */
} Player;

/* Load the player texture and set its initial position. */
void player_init(Player *player, SDL_Renderer *renderer);

/* Sample the keyboard every frame and set vx/vy accordingly. */
void player_handle_input(Player *player);

/* Move the player by velocity × dt and clamp to the window edges. */
void player_update(Player *player, float dt);

/* Draw the player sprite at its current position. */
void player_render(Player *player, SDL_Renderer *renderer);

/* Release the player's GPU texture. */
void player_cleanup(Player *player);
