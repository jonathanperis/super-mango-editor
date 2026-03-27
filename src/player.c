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
/* Width and height of one sprite frame in the sheet (pixels). */
#define FRAME_W 48
#define FRAME_H 48

void player_init(Player *player, SDL_Renderer *renderer) {
    /*
     * IMG_LoadTexture — decode the PNG sprite sheet and upload it to the GPU.
     * The sheet is 192×288 px and contains a 4-column × 6-row grid of 48×48
     * frames. We only draw one frame at a time using a source clipping rect.
     */
    player->texture = IMG_LoadTexture(renderer, "assets/Player.png");
    if (!player->texture) {
        fprintf(stderr, "Failed to load Player.png: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * frame — the source rectangle: which 48×48 cell to cut from the sheet.
     * {x=0, y=0} → top-left cell, which is the first idle/standing pose.
     * We keep frame.w and frame.h constant at FRAME_W/FRAME_H for now.
     */
    player->frame.x = 0;
    player->frame.y = 0;
    player->frame.w = FRAME_W;
    player->frame.h = FRAME_H;

    /* The on-screen display size matches the frame size exactly. */
    player->w = FRAME_W;
    player->h = FRAME_H;

    /*
     * Place the player horizontally centered, sitting on top of the floor.
     * FLOOR_Y is the top edge of the grass tiles, so the player's bottom
     * edge (y + h) should equal FLOOR_Y at rest.
     */
    player->x        = (GAME_W - player->w) / 2.0f;
    player->y        = (float)(FLOOR_Y - player->h);
    player->vx       = 0.0f;
    player->vy       = 0.0f;   /* start stationary; gravity will pull down   */
    player->speed    = 160.0f; /* horizontal speed: 160 logical px per second */
    player->on_ground = 1;     /* starts on the floor                         */
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
     * Only horizontal input is handled here.
     * Vertical movement is driven by gravity in player_update, not by keys.
     * Reset vx each frame so the player stops the moment the key is released.
     */
    player->vx = 0.0f;

    /* Left/Right: arrow keys and WASD both work */
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) player->vx -= player->speed;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) player->vx += player->speed;
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
     * dst — where on screen the sprite will appear.
     * x/y are cast from float to int because SDL works in whole pixels.
     * w/h match the frame size (FRAME_W × FRAME_H = 48×48).
     */
    SDL_Rect dst = {
        .x = (int)player->x,
        .y = (int)player->y,
        .w = player->w,
        .h = player->h
    };

    /*
     * SDL_RenderCopy — copy a region of the texture onto the back buffer.
     *   renderer      → the drawing context
     *   texture       → the full sprite sheet (192×288) on the GPU
     *   &player->frame → source clipping rect: selects the 48×48 frame
     *                    we want from the sheet (currently always frame 0)
     *   &dst          → destination rect: where/how big to draw on screen
     */
    SDL_RenderCopy(renderer, player->texture, &player->frame, &dst);
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
