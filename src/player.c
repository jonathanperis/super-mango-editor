/*
 * player.c — Implementation of player init, input, physics, rendering, and cleanup.
 */

#include <SDL_image.h>  /* IMG_LoadTexture */
#include <SDL_mixer.h>  /* Mix_Chunk, Mix_PlayChannel */
#include <stdio.h>
#include <stdlib.h>

#include "player.h"
#include "game.h"   /* GAME_W, GAME_H, FLOOR_Y, GRAVITY (for physics and clamping) */

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
void player_handle_input(Player *player, Mix_Chunk *snd_jump) {
    /*
     * SDL_GetKeyboardState returns a pointer to an array of key states.
     * Each element is 1 if that key is currently held, 0 if not.
     * Indexed by SDL_SCANCODE_* values (hardware-based, layout-independent).
     * Passing NULL means "use SDL's internal state array".
     */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    /*
     * Horizontal movement: left/right arrow keys and A/D both work.
     * Reset vx to 0 each frame so the player stops when keys are released.
     */
    player->vx = 0.0f;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) player->vx -= player->speed;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) player->vx += player->speed;

    /*
     * Jump: Space, W, or ↑ — only allowed while standing on the floor.
     * Setting on_ground = 0 ensures this block only fires once per jump;
     * subsequent frames while the key is held skip it because on_ground is.
     * vy is set to a negative value (up is negative in SDL screen-space).
     */
    if (player->on_ground &&
        (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])) {
        player->vy        = -520.0f;  /* upward impulse in logical px/s */
        player->on_ground  = 0;
        if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
    }
}

/* ------------------------------------------------------------------ */

/*
 * player_update — Apply gravity and velocity to position, handle floor/ceiling collisions.
 *
 * dt (delta time) is the time in seconds since the last frame (e.g. 0.016).
 * Multiplying velocity (px/s) by dt (s) gives displacement in pixels.
 * This makes movement speed identical regardless of frame rate.
 */
void player_update(Player *player, float dt) {
    /*
     * Gravity: accelerate downward each frame while the player is airborne.
     * GRAVITY is in px/s² so we multiply by dt to get px/s added this frame.
     */
    if (!player->on_ground) {
        player->vy += GRAVITY * dt;
    }

    player->x += player->vx * dt;   /* move horizontally */
    player->y += player->vy * dt;   /* move vertically   */

    /*
     * Floor collision — snap to the grass surface and stop falling.
     * FLOOR_Y is the top edge of the grass tiles in logical coordinates.
     * The player's bottom edge is y + h; when it reaches FLOOR_Y, land.
     */
    if (player->y >= (float)(FLOOR_Y - player->h)) {
        player->y        = (float)(FLOOR_Y - player->h);
        player->vy       = 0.0f;
        player->on_ground = 1;
    }

    /*
     * Horizontal clamp — keep the player inside the logical canvas (GAME_W).
     * All coordinates live in logical (400×300) space, not OS window space.
     */
    if (player->x < 0.0f)               player->x = 0.0f;
    if (player->x > GAME_W - player->w) player->x = (float)(GAME_W - player->w);

    /* Ceiling clamp — stop upward movement at the top of the canvas. */
    if (player->y < 0.0f) {
        player->y  = 0.0f;
        player->vy = 0.0f;
    }
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
