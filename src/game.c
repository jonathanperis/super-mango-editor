/*
 * game.c — Window, renderer, background, and main game loop.
 */

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "player.h"
#include "platform.h"
#include "water.h"
#include "fog.h"
#include "spider.h"
#include "coin.h"
#include "hud.h"

/* ------------------------------------------------------------------ */

/*
 * game_init — Set up everything the game needs before the loop starts.
 *
 * Called once at startup. Creates the OS window, the GPU renderer,
 * loads the background image, and initialises the player.
 */
void game_init(GameState *gs) {
    /*
     * SDL_CreateWindow — ask the OS to create a window.
     *   WINDOW_TITLE            → text shown in the title bar
     *   SDL_WINDOWPOS_CENTERED  → center the window on the monitor (x and y)
     *   WINDOW_W / WINDOW_H     → 800×600 pixels
     *   SDL_WINDOW_SHOWN        → make it visible immediately
     */
    gs->window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );
    if (!gs->window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * SDL_CreateRenderer — create a 2D drawing context tied to the window.
     *   -1 → let SDL pick the best available GPU driver automatically
     *   SDL_RENDERER_ACCELERATED  → use the GPU (not software fallback)
     *   SDL_RENDERER_PRESENTVSYNC → lock present() to the monitor refresh rate
     *                               (prevents screen tearing and runaway CPU)
     */
    gs->renderer = SDL_CreateRenderer(
        gs->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!gs->renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * SDL_RenderSetLogicalSize — define the internal canvas resolution.
     * All SDL render calls now use GAME_W × GAME_H coordinates (400×300).
     * SDL scales this canvas up to the OS window size (800×600) automatically,
     * giving every pixel a 2×2 block on screen — the chunky pixel-art look.
     */
    SDL_RenderSetLogicalSize(gs->renderer, GAME_W, GAME_H);

    /*
     * IMG_LoadTexture — decode a PNG file and upload it to GPU memory.
     * A "texture" is an image that lives on the GPU and can be drawn very fast.
     * The path is relative to where the binary is run from (repo root).
     */
    /* Load the forest background (stretched to fill the window at render time) */
    gs->background = IMG_LoadTexture(gs->renderer, "assets/Forest_Background_0.png");
    if (!gs->background) {
        fprintf(stderr, "Failed to load Forest_Background_0.png: %s\n", IMG_GetError());
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * Load the grass tile. This single 48×48 texture will be repeated
     * (tiled) across the full window width to form the floor.
     */
    gs->floor_tile = IMG_LoadTexture(gs->renderer, "assets/Grass_Tileset.png");
    if (!gs->floor_tile) {
        fprintf(stderr, "Failed to load Grass_Tileset.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * Load the one-way platform tile texture.
     * Grass_Oneway.png is a 48×48 grass block used to build pillar stacks.
     * The same texture is reused for every tile in every platform, so we
     * load it once here and pass it to platforms_render each frame.
     */
    gs->platform_tex = IMG_LoadTexture(gs->renderer, "assets/Grass_Oneway.png");
    if (!gs->platform_tex) {
        fprintf(stderr, "Failed to load Grass_Oneway.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->floor_tile);
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /* Populate the platforms array with the two pillar definitions */
    platforms_init(gs->platforms, &gs->platform_count);

    /* Load the animated water strip texture and reset scroll state */
    water_init(&gs->water, gs->renderer);

    /*
     * Load the shared spider texture.  All spider instances blit from
     * this single texture using different source rects per frame.
     */
    gs->spider_tex = IMG_LoadTexture(gs->renderer, "assets/Spider_1.png");
    if (!gs->spider_tex) {
        fprintf(stderr, "Failed to load Spider_1.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    spiders_init(gs->spiders, &gs->spider_count);

    /*
     * Load the shared coin texture.  All coin instances blit from this
     * single texture using the full image as the source rect.
     */
    gs->coin_tex = IMG_LoadTexture(gs->renderer, "assets/Coin.png");
    if (!gs->coin_tex) {
        fprintf(stderr, "Failed to load Coin.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    coins_init(gs->coins, &gs->coin_count);

    /*
     * Load the jump sound effect. Mix_LoadWAV decodes the WAV into a
     * Mix_Chunk that can be played on any available mixer channel.
     * Assets path is relative to where the binary is run (repo root).
     */
    gs->snd_jump = Mix_LoadWAV("sounds/jump.wav");
    if (!gs->snd_jump) {
        fprintf(stderr, "Failed to load jump.wav: %s\n", Mix_GetError());
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * Load the coin pickup SFX.
     * Non-fatal: if loading fails, gameplay continues without this effect.
     */
    gs->snd_coin = Mix_LoadWAV("sounds/coin.wav");
    if (!gs->snd_coin) {
        fprintf(stderr, "Warning: Failed to load coin.wav: %s\n", Mix_GetError());
    }

    /*
     * Load the player-hit SFX.
     * Non-fatal: if loading fails, gameplay continues without this effect.
     */
    gs->snd_hit = Mix_LoadWAV("sounds/hit.wav");
    if (!gs->snd_hit) {
        fprintf(stderr, "Warning: Failed to load hit.wav: %s\n", Mix_GetError());
    }

    /*
     * Load and immediately start the background music track.
     * Mix_Music is the streaming type used for long audio (MP3/OGG/FLAC).
     * Unlike Mix_Chunk, it streams from disk rather than loading all at once,
     * which keeps memory usage low for large files.
     * Mix_PlayMusic(-1) means loop forever until Mix_HaltMusic() is called.
     */
    gs->music = Mix_LoadMUS("sounds/water-ambience.ogg");
    if (!gs->music) {
        fprintf(stderr, "Failed to load water-ambience.ogg: %s\n", Mix_GetError());
        Mix_FreeChunk(gs->snd_jump);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    Mix_PlayMusic(gs->music, -1);   /* -1 = loop indefinitely                */
    /*
     * Mix_VolumeMusic — set the music playback volume.
     * Range: 0 (silent) to MIX_MAX_VOLUME (128, full volume).
     * 13/128 ≈ 10%, keeping the ambient track soft so it doesn't
     * compete with sound effects.
     */
    Mix_VolumeMusic(13);

    /* Set up the player (loads texture, sets initial position on the floor) */
    player_init(&gs->player, gs->renderer);

    /* Load fog textures and spawn the first mist wave */
    fog_init(&gs->fog, gs->renderer);

    /* Load the HUD font and heart icon texture */
    hud_init(&gs->hud, gs->renderer);

    /* Initialise the health/lives/score system */
    gs->hearts         = MAX_HEARTS;
    gs->lives          = DEFAULT_LIVES;
    gs->score          = 0;
    gs->coins_for_heart = 0;

    /* Signal the loop to start running */
    gs->running = 1;
}

/* ------------------------------------------------------------------ */

/*
 * game_loop — The heart of the game. Runs until gs->running == 0.
 *
 * Every iteration of the while loop is one "frame". Each frame:
 *   1. Calculate how much time has passed (delta time).
 *   2. Process all pending OS/input events.
 *   3. Update game state (player position, etc.).
 *   4. Draw everything to the screen.
 */
void game_loop(GameState *gs) {
    /*
     * frame_ms — how many milliseconds one frame should take at TARGET_FPS.
     * e.g. 1000ms / 60fps ≈ 16ms per frame.
     * Used as a manual frame cap fallback if VSync is not active.
     */
    const Uint32 frame_ms = 1000 / TARGET_FPS;

    /* Record the timestamp at the start of the very first frame */
    Uint64 prev = SDL_GetTicks64();

    while (gs->running) {
        /*
         * Delta time (dt) — seconds elapsed since the previous frame.
         * SDL_GetTicks64() returns milliseconds since SDL was initialised.
         * Dividing by 1000 converts to seconds (a float like 0.016).
         * We multiply velocities by dt so movement is frame-rate independent.
         */
        Uint64 now = SDL_GetTicks64();
        float  dt  = (float)(now - prev) / 1000.0f;
        prev = now;

        /* ---- 1. Events ------------------------------------------- */
        /*
         * SDL_PollEvent — drain the event queue one event at a time.
         * Events are things the OS tells us about: key presses, mouse clicks,
         * the user clicking the window's close button, etc.
         * Returns 1 while there are events to process, 0 when the queue is empty.
         */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                /* User clicked the window's X button */
                gs->running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                /* A key was pressed this frame */
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    gs->running = 0;
                }
            }
        }

        /* ---- 2. Update ------------------------------------------- */
        /* Read the keyboard and set the player's velocity for this frame */
        player_handle_input(&gs->player, gs->snd_jump);
        /* Move the player, check floor + one-way platform collisions */
        player_update(&gs->player, dt, gs->platforms, gs->platform_count);
        /* Move spiders along their patrol paths and advance their animation */
        spiders_update(gs->spiders, gs->spider_count, dt);

        /*
         * Player–spider collision.
         *
         * Count down the invincibility timer first.  While it is positive the
         * player is still blinking from a previous hit and cannot be hurt again.
         * When the timer reaches zero, test each spider's render rect against
         * the player's physics hitbox (AABB overlap).  On contact, start a new
         * 1.5-second invincibility window.
         */
        if (gs->player.hurt_timer > 0.0f) {
            gs->player.hurt_timer -= dt;
            if (gs->player.hurt_timer < 0.0f)
                gs->player.hurt_timer = 0.0f;
        } else {
            SDL_Rect phit = player_get_hitbox(&gs->player);
            for (int i = 0; i < gs->spider_count; i++) {
                const Spider *s = &gs->spiders[i];
                SDL_Rect shit = {
                    (int)s->x,
                    FLOOR_Y - SPIDER_ART_H,
                    SPIDER_FRAME_W,
                    SPIDER_ART_H
                };
                /* SDL_HasIntersection returns SDL_TRUE when the two rects overlap */
                if (SDL_HasIntersection(&phit, &shit)) {
                    gs->player.hurt_timer = 1.5f;   /* 1.5 s of invincibility */

                    /* Play hit SFX once when damage is applied. */
                    if (gs->snd_hit) {
                        Mix_PlayChannel(-1, gs->snd_hit, 0);
                    }

                    gs->hearts--;
                    if (gs->hearts <= 0) {
                        gs->lives--;
                        if (gs->lives <= 0) {
                            /* Game over: restart everything */
                            gs->lives          = DEFAULT_LIVES;
                            gs->score          = 0;
                            gs->coins_for_heart = 0;
                        }
                        gs->hearts = MAX_HEARTS;
                        player_reset(&gs->player);
                        coins_init(gs->coins, &gs->coin_count);
                        spiders_init(gs->spiders, &gs->spider_count);
                    }
                    break;
                }
            }
        }

        /*
         * Coin collection.
         *
         * Test the player's physics hitbox against every active coin.
         * On overlap: deactivate the coin, award COIN_SCORE points, and
         * increment the coins-toward-heart counter.  When the counter
         * reaches COINS_PER_HEART, restore one heart (if below max).
         */
        {
            SDL_Rect phit = player_get_hitbox(&gs->player);
            for (int i = 0; i < gs->coin_count; i++) {
                if (!gs->coins[i].active) continue;
                SDL_Rect cbox = {
                    (int)gs->coins[i].x, (int)gs->coins[i].y,
                    COIN_DISPLAY_W, COIN_DISPLAY_H
                };
                if (SDL_HasIntersection(&phit, &cbox)) {
                    gs->coins[i].active = 0;

                    /* Play coin SFX immediately when a coin is collected. */
                    if (gs->snd_coin) {
                        Mix_PlayChannel(-1, gs->snd_coin, 0);
                    }

                    gs->score += COIN_SCORE;
                    gs->coins_for_heart++;
                    if (gs->coins_for_heart >= COINS_PER_HEART) {
                        gs->coins_for_heart = 0;
                        if (gs->hearts < MAX_HEARTS)
                            gs->hearts++;
                    }
                }
            }
        }

        /* Advance the water scroll offset */
        water_update(&gs->water, dt);
        /* Advance the fog wave positions and spawn the next wave if it is time */
        fog_update(&gs->fog, dt);

        /* ---- 3. Render ------------------------------------------- */
        /*
         * SDL_RenderClear — fill the back buffer with the draw colour.
         * We always clear before drawing to avoid leftover pixels from the
         * previous frame showing through.
         */
        SDL_RenderClear(gs->renderer);

        /*
         * Draw the background stretched to cover the entire logical canvas (GAME_W×GAME_H).
         * SDL_RenderSetLogicalSize handles scaling to the OS window automatically.
         */
        SDL_Rect bg = {0, 0, GAME_W, GAME_H};
        SDL_RenderCopy(gs->renderer, gs->background, NULL, &bg);

        /*
         * 9-slice floor rendering.
         *
         * The 48×48 Grass_Tileset.png is divided into a 3×3 grid of 16×16 pieces
         * (TILE_SIZE / 3 = 16). Each piece has a role:
         *
         *   [TL][TC][TR]   row 0  y= 0..15  ← grass edge
         *   [ML][MC][MR]   row 1  y=16..31  ← dirt interior
         *   [BL][BC][BR]   row 2  y=32..47  ← floor base edge
         *
         * Piece selection per screen position:
         *   • Leftmost  column (tx == 0)           → col 0 (left  cap: TL/ML/BL)
         *   • Rightmost column (tx + P >= GAME_W)  → col 2 (right cap: TR/MR/BR)
         *   • All other columns                    → col 1 (center fill: TC/MC/BC)
         *
         *   • First row  (ty == FLOOR_Y)           → row 0 (grass edge)
         *   • Last  row  (ty + P >= GAME_H)        → row 2 (base edge)
         *   • Middle rows                          → row 1 (dirt interior)
         *
         * GAME_W (400) and floor height (48) are both exact multiples of P (16),
         * so every piece fits without needing a partial-pixel crop.
         * The cap pieces carry the black outline border; the center pieces are
         * pure interior texture — no seams appear between adjacent pieces.
         */
        const int P = TILE_SIZE / 3;   /* 9-slice piece size: 16 px */

        for (int ty = FLOOR_Y; ty < GAME_H; ty += P) {
            /* Choose which row of the 9-slice to sample */
            int piece_row;
            if (ty == FLOOR_Y)        piece_row = 0;   /* top: grass edge  */
            else if (ty + P >= GAME_H) piece_row = 2;  /* bottom: base edge */
            else                      piece_row = 1;   /* middle: dirt fill */

            for (int tx = 0; tx < GAME_W; tx += P) {
                /* Choose which column of the 9-slice to sample */
                int piece_col;
                if (tx == 0)            piece_col = 0;  /* left cap  */
                else if (tx + P >= GAME_W) piece_col = 2; /* right cap */
                else                    piece_col = 1;  /* center    */

                /*
                 * src — the 16×16 region cut from the tile sheet.
                 *   x = piece_col × P selects the column (0, 16, or 32).
                 *   y = piece_row × P selects the row    (0, 16, or 32).
                 * dst — where on screen this piece is drawn (1:1 pixel mapping).
                 */
                SDL_Rect src = {piece_col * P, piece_row * P, P, P};
                SDL_Rect dst = {tx, ty, P, P};
                SDL_RenderCopy(gs->renderer, gs->floor_tile, &src, &dst);
            }
        }

        /*
         * Draw the one-way platforms after the floor but before the player,
         * so the player sprite appears in front of the pillar tiles.
         */
        platforms_render(gs->platforms, gs->platform_count,
                         gs->renderer, gs->platform_tex);

        /* Draw coins on top of the platforms, before the water and player */
        coins_render(gs->coins, gs->coin_count,
                     gs->renderer, gs->coin_tex);

        /*
         * Draw the water strip on top of the floor/platforms.
         * The full 384-px sheet scrolls rightward as a seamless loop.
         */
        water_render(&gs->water, gs->renderer);

        /* Draw spiders on top of the water strip, before the player */
        spiders_render(gs->spiders, gs->spider_count,
                       gs->renderer, gs->spider_tex);

        /* Draw the player sprite on top of everything */
        player_render(&gs->player, gs->renderer);

        /* Draw fog/mist as the topmost layer — rendered after the player */
        fog_render(&gs->fog, gs->renderer);

        /* Draw the HUD overlay on top of everything (hearts, lives, score) */
        hud_render(&gs->hud, gs->renderer, gs->player.texture,
                   gs->hearts, gs->lives, gs->score);

        /*
         * SDL_RenderPresent — swap the back buffer to the screen.
         * Everything drawn since RenderClear was on a hidden buffer.
         * This call makes it visible instantly, preventing flicker.
         * With VSync enabled, this call also blocks until the monitor
         * is ready for the next frame (typically ~16ms at 60 Hz).
         */
        SDL_RenderPresent(gs->renderer);

        /*
         * Manual frame cap fallback: if the frame finished before frame_ms ms
         * (e.g. VSync is off), sleep for the remaining time.
         * SDL_Delay sleeps the CPU so we don't burn 100% for no reason.
         */
        Uint64 elapsed = SDL_GetTicks64() - now;
        if (elapsed < frame_ms) {
            SDL_Delay((Uint32)(frame_ms - elapsed));
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * game_cleanup — Free every resource owned by the game.
 *
 * Always destroy in reverse init order, because later objects may
 * depend on earlier ones (e.g. a texture requires the renderer to exist).
 * After destroying, we set pointers to NULL so accidental double-frees
 * are safe (SDL_Destroy* and free() on NULL are no-ops).
 */
void game_cleanup(GameState *gs) {
    /* Free HUD resources (font + star texture, renderer-dependent) */
    hud_cleanup(&gs->hud);

    /* Free fog textures (renderer-dependent, free before renderer) */
    fog_cleanup(&gs->fog);

    /* Free the player's texture first (also renderer-dependent) */
    player_cleanup(&gs->player);

    if (gs->music) {
        Mix_HaltMusic();           /* stop playback before freeing           */
        Mix_FreeMusic(gs->music);
        gs->music = NULL;
    }

    if (gs->snd_jump) {
        Mix_FreeChunk(gs->snd_jump);
        gs->snd_jump = NULL;
    }

    if (gs->snd_coin) {
        Mix_FreeChunk(gs->snd_coin);
        gs->snd_coin = NULL;
    }

    if (gs->snd_hit) {
        Mix_FreeChunk(gs->snd_hit);
        gs->snd_hit = NULL;
    }

    water_cleanup(&gs->water);

    if (gs->spider_tex) {
        SDL_DestroyTexture(gs->spider_tex);
        gs->spider_tex = NULL;
    }

    if (gs->coin_tex) {
        SDL_DestroyTexture(gs->coin_tex);
        gs->coin_tex = NULL;
    }

    if (gs->platform_tex) {
        SDL_DestroyTexture(gs->platform_tex);
        gs->platform_tex = NULL;
    }

    if (gs->floor_tile) {
        SDL_DestroyTexture(gs->floor_tile);
        gs->floor_tile = NULL;
    }

    if (gs->background) {
        SDL_DestroyTexture(gs->background);  /* release GPU memory */
        gs->background = NULL;
    }

    if (gs->renderer) {
        SDL_DestroyRenderer(gs->renderer);
        gs->renderer = NULL;
    }

    if (gs->window) {
        SDL_DestroyWindow(gs->window);
        gs->window = NULL;
    }
}
