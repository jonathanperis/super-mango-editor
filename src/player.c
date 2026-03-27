/*
 * player.c — Implementation of player init, input, physics, rendering, and cleanup.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <stdio.h>
#include <stdlib.h>

#include "player.h"
#include "game.h"   /* WINDOW_W, WINDOW_H (for centering and clamping) */

/* ------------------------------------------------------------------ */

/*
 * player_init — Load the sprite and place the player in the center of the window.
 */
void player_init(Player *player, SDL_Renderer *renderer) {
    /*
     * IMG_LoadTexture — a convenience function from SDL2_image.
     * It decodes the PNG file and uploads the pixel data to the GPU in one call.
     * The returned SDL_Texture* is a handle to that GPU memory.
     */
    player->texture = IMG_LoadTexture(renderer, "assets/Player.png");
    if (!player->texture) {
        fprintf(stderr, "Failed to load Player.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * SDL_QueryTexture — ask SDL for the texture's metadata.
     * The first two NULLs say "we don't need the pixel format or access mode".
     * &player->w and &player->h receive the sprite's dimensions in pixels.
     * This way we never hardcode the sprite size — it comes from the PNG itself.
     */
    SDL_QueryTexture(player->texture, NULL, NULL, &player->w, &player->h);

    /*
     * Center the player in the window.
     * (WINDOW_W - player->w) is the total horizontal space not occupied by the sprite.
     * Dividing by 2 gives the left margin needed to center it.
     * The 2.0f suffix makes it a float division so we don't lose decimals.
     */
    player->x     = (WINDOW_W - player->w) / 2.0f;
    player->y     = (WINDOW_H - player->h) / 2.0f;
    player->vx    = 0.0f;   /* start stationary */
    player->vy    = 0.0f;
    player->speed = 200.0f; /* 200 pixels per second */
}

/* ------------------------------------------------------------------ */

/*
 * player_handle_input — Sample the keyboard and set the player's velocity.
 *
 * Called once per frame, before player_update.
 *
 * We use SDL_GetKeyboardState instead of key-press events because
 * it tells us which keys are held RIGHT NOW, giving smooth, continuous
 * movement rather than one-shot movement on the moment of press.
 */
void player_handle_input(Player *player) {
    /*
     * SDL_GetKeyboardState returns a pointer to an array of key states.
     * Each element is 1 if that key is currently held, 0 if not.
     * Indexed by SDL_SCANCODE_* values (hardware-based, layout-independent).
     * Passing NULL means "use SDL's internal state array".
     */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    /*
     * Reset velocity to zero at the start of every frame.
     * This means the player only moves while a key is held.
     * Releasing all keys immediately stops movement (no sliding).
     */
    player->vx = 0.0f;
    player->vy = 0.0f;

    /* Support both arrow keys and WASD — either works */
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) player->vx -= player->speed;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) player->vx += player->speed;
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) player->vy -= player->speed;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) player->vy += player->speed;
    /*
     * Note: in SDL's coordinate system, Y increases downward.
     * So vy < 0 moves the sprite UP the screen, vy > 0 moves it DOWN.
     */
}

/* ------------------------------------------------------------------ */

/*
 * player_update — Apply velocity to position, then enforce window boundaries.
 *
 * dt (delta time) is the time in seconds since the last frame (e.g. 0.016).
 * Multiplying velocity (px/s) by dt (s) gives displacement in pixels.
 * This makes movement speed identical regardless of frame rate.
 */
void player_update(Player *player, float dt) {
    player->x += player->vx * dt;   /* move horizontally */
    player->y += player->vy * dt;   /* move vertically   */

    /*
     * Clamp — keep the player fully inside the window.
     * Left/top edge: position must be >= 0.
     * Right/bottom edge: position must be <= window size minus sprite size
     *   (because x/y refers to the top-left corner of the sprite).
     */
    if (player->x < 0.0f)                 player->x = 0.0f;
    if (player->y < 0.0f)                 player->y = 0.0f;
    if (player->x > WINDOW_W - player->w) player->x = (float)(WINDOW_W - player->w);
    if (player->y > WINDOW_H - player->h) player->y = (float)(WINDOW_H - player->h);
}

/* ------------------------------------------------------------------ */

/*
 * player_render — Draw the player sprite at its current position.
 */
void player_render(Player *player, SDL_Renderer *renderer) {
    /*
     * SDL_Rect describes a rectangle on screen: {x, y, width, height}.
     * We cast x/y from float to int here — SDL works in whole pixels.
     * The sprite is drawn at its natural size (w × h from the PNG).
     */
    SDL_Rect dst = {
        .x = (int)player->x,
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    /*
     * SDL_RenderCopy — copy a texture onto the renderer's back buffer.
     *   renderer    → the drawing context
     *   texture     → source image (on the GPU)
     *   NULL        → source rect: NULL means "use the entire texture"
     *   &dst        → destination rect: where/how big to draw it on screen
     */
    SDL_RenderCopy(renderer, player->texture, NULL, &dst);
}

/* ------------------------------------------------------------------ */

/*
 * player_cleanup — Release GPU memory held by the player's texture.
 *
 * Must be called before the renderer is destroyed, because SDL_Texture
 * objects are owned by the renderer that created them.
 */
void player_cleanup(Player *player) {
    if (player->texture) {
        SDL_DestroyTexture(player->texture);
        player->texture = NULL;   /* guard against accidental double-free */
    }
}
