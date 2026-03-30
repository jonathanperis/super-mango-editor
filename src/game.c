/*
 * game.c — Window, renderer, background, and main game loop.
 */

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>   /* sqrtf — used by apply_damage push impulse */

#include "game.h"
#include "player.h"
#include "platform.h"
#include "water.h"
#include "fog.h"
#include "spider.h"
#include "fish.h"
#include "coin.h"
#include "vine.h"
#include "bouncepad.h"
#include "hud.h"
#include "parallax.h"
#include "rail.h"
#include "spike_block.h"
#include "float_platform.h"

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
     * Nearest-neighbour pixel scaling — must be set before SDL_CreateRenderer.
     * Value "0" = SDL_ScaleModeNearest: each source pixel maps to an exact
     * NxN block of destination pixels, preserving the crisp look of pixel art.
     * The default ("linear") blurs sprites when they are scaled, which makes
     * small pixel-art sprites (e.g. 16×16 fish → 48×48 on screen) look wrong.
     */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

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
     * Load the multi-layer parallax background system.
     * Each layer is non-fatal: missing PNGs print a warning and are skipped
     * at render time.  The renderer must exist before this call because
     * IMG_LoadTexture uploads data to the GPU immediately.
     */
    parallax_init(&gs->parallax, gs->renderer);

    /*
     * Load the grass tile. This single 48×48 texture will be repeated
     * (tiled) across the full window width to form the floor.
     */
    gs->floor_tile = IMG_LoadTexture(gs->renderer, "assets/Grass_Tileset.png");
    if (!gs->floor_tile) {
        fprintf(stderr, "Failed to load Grass_Tileset.png: %s\n", IMG_GetError());
        parallax_cleanup(&gs->parallax);
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
        parallax_cleanup(&gs->parallax);
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
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    spiders_init(gs->spiders, &gs->spider_count);

    /*
     * Load the jumping spider texture (Spider_2.png, same layout as Spider_1).
     * Fatal — a gameplay enemy that the player must be able to see.
     */
    gs->jumping_spider_tex = IMG_LoadTexture(gs->renderer, "assets/Spider_2.png");
    if (!gs->jumping_spider_tex) {
        fprintf(stderr, "Failed to load Spider_2.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    jumping_spiders_init(gs->jumping_spiders, &gs->jumping_spider_count);

    /*
     * Load the bird texture (Bird_2.png — slow bird).
     * Non-fatal would be acceptable but birds are gameplay enemies, so fatal.
     */
    gs->bird_tex = IMG_LoadTexture(gs->renderer, "assets/Bird_2.png");
    if (!gs->bird_tex) {
        fprintf(stderr, "Failed to load Bird_2.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->jumping_spider_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    birds_init(gs->birds, &gs->bird_count);

    /*
     * Load the faster bird texture (Bird_1.png — fast bird).
     */
    gs->faster_bird_tex = IMG_LoadTexture(gs->renderer, "assets/Bird_1.png");
    if (!gs->faster_bird_tex) {
        fprintf(stderr, "Failed to load Bird_1.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->bird_tex);
        SDL_DestroyTexture(gs->jumping_spider_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    faster_birds_init(gs->faster_birds, &gs->faster_bird_count);

    /*
     * Load the shared fish texture. Fish live in the water lane and use
     * the same texture for all instances, just like the spider enemy.
     */
    gs->fish_tex = IMG_LoadTexture(gs->renderer, "assets/Fish_2.png");
    if (!gs->fish_tex) {
        fprintf(stderr, "Failed to load Fish_2.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->jumping_spider_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    fish_init(gs->fish, &gs->fish_count);

    /*
     * Load the shared coin texture.  All coin instances blit from this
     * single texture using the full image as the source rect.
     */
    gs->coin_tex = IMG_LoadTexture(gs->renderer, "assets/Coin.png");
    if (!gs->coin_tex) {
        fprintf(stderr, "Failed to load Coin.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->fish_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    coins_init(gs->coins, &gs->coin_count);

    /*
     * Load the shared vine texture.  Vines are static scenery; loading
     * failure is non-fatal — the decorations are simply skipped.
     */
    gs->vine_tex = IMG_LoadTexture(gs->renderer, "assets/Vine.png");
    if (!gs->vine_tex) {
        fprintf(stderr, "Warning: Failed to load Vine.png: %s\n", IMG_GetError());
    }
    vine_init(gs->vines, &gs->vine_count);

    /*
     * Load the bouncepad texture and initialise pad placements.
     *
     * Bouncepad_Wood.png is a 144×48 single-row sheet with 3 frames of 48×48.
     * Fatal if missing — the pad is a gameplay element, not just decoration.
     */
    gs->bouncepad_tex = IMG_LoadTexture(gs->renderer, "assets/Bouncepad_Wood.png");
    if (!gs->bouncepad_tex) {
        fprintf(stderr, "Failed to load Bouncepad_Wood.png: %s\n", IMG_GetError());
        if (gs->vine_tex) SDL_DestroyTexture(gs->vine_tex);
        SDL_DestroyTexture(gs->coin_tex);
        SDL_DestroyTexture(gs->fish_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }
    bouncepads_init(gs->bouncepads, &gs->bouncepad_count);

    /*
     * Load the bouncepad sound effect.
     * Mix_LoadWAV handles WAV and, with SDL2_mixer ≥ 2.0, also MP3.
     * Non-fatal: gameplay continues without audio if loading fails.
     */
    gs->snd_spring = Mix_LoadWAV("sounds/bouncepad.mp3");
    if (!gs->snd_spring) {
        fprintf(stderr, "Warning: Failed to load bouncepad.mp3: %s\n", Mix_GetError());
    }
    /*
     * Load the rail tile texture.  Rails.png is a 64×64 sprite sheet divided
     * into a 4×4 grid of 16×16 tiles.  Non-fatal: missing texture prints a
     * warning and rail_render silently skips the draw each frame.
     */
    gs->rail_tex = IMG_LoadTexture(gs->renderer, "assets/Rails.png");
    if (!gs->rail_tex) {
        fprintf(stderr, "Warning: Failed to load Rails.png: %s\n", IMG_GetError());
    }
    rail_init(gs->rails, &gs->rail_count);

    /*
     * Load the spike block texture.  Spike_Block.png is a single 16×16 frame.
     * Non-fatal for the same reason as rail_tex.
     */
    gs->spike_block_tex = IMG_LoadTexture(gs->renderer, "assets/Spike_Block.png");
    if (!gs->spike_block_tex) {
        fprintf(stderr, "Warning: Failed to load Spike_Block.png: %s\n", IMG_GetError());

    }
    spike_blocks_init(gs->spike_blocks, &gs->spike_block_count, gs->rails);

    /*
     * Load the float-platform texture.  Platform.png is a 48×16 px sprite
     * divided into three 16×16 horizontal slices (left cap, centre fill,
     * right cap).  Non-fatal: platforms are skipped at render time if NULL.
     */
    gs->float_platform_tex = IMG_LoadTexture(gs->renderer, "assets/Platform.png");
    if (!gs->float_platform_tex) {
        fprintf(stderr, "Warning: Failed to load Platform.png: %s\n", IMG_GetError());
    }
    float_platforms_init(gs->float_platforms, &gs->float_platform_count, gs->rails);

    /*
     * Load the bridge tile texture (Bridge.png, 16×16).
     * Non-fatal — the game can run without it (warning only).
     */
    gs->bridge_tex = IMG_LoadTexture(gs->renderer, "assets/Bridge.png");
    if (!gs->bridge_tex) {
        fprintf(stderr, "Warning: Failed to load Bridge.png: %s\n", IMG_GetError());
    }
    bridges_init(gs->bridges, &gs->bridge_count);

    /*
     * Load the jump sound effect. Mix_LoadWAV decodes the WAV into a
     * Mix_Chunk that can be played on any available mixer channel.
     * Assets path is relative to where the binary is run (repo root).
     */
    gs->snd_jump = Mix_LoadWAV("sounds/jump.wav");
    if (!gs->snd_jump) {
        fprintf(stderr, "Failed to load jump.wav: %s\n", Mix_GetError());
        SDL_DestroyTexture(gs->coin_tex);
        SDL_DestroyTexture(gs->fish_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
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
        SDL_DestroyTexture(gs->coin_tex);
        SDL_DestroyTexture(gs->fish_tex);
        SDL_DestroyTexture(gs->spider_tex);
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
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

    /* Camera starts at the far-left edge of the world. */
    gs->camera.x = 0.0f;

    /* Load fog textures and spawn the first mist wave */
    fog_init(&gs->fog, gs->renderer);

    /* Load the HUD font and heart icon texture */
    hud_init(&gs->hud, gs->renderer);

    /* Initialise the debug overlay if --debug was passed on the CLI */
    if (gs->debug_mode) debug_init(&gs->debug);

    /*
     * Sea gaps — holes in the ground floor that expose the water below.
     * Each gap is SEA_GAP_W (32 px) wide, positioned to avoid overlapping
     * platforms, bouncepads, and ground coins.
     */
    gs->sea_gaps[0] = 0;       /* left world edge  (screen 1 start)   */
    gs->sea_gaps[1] = 192;     /* screen 1 mid     (free zone 187–255)*/
    gs->sea_gaps[2] = 560;     /* screen 2 mid     (free zone 549–594)*/
    gs->sea_gaps[3] = 928;     /* screen 3 mid     (16px-aligned, after platform at 880) */
    gs->sea_gaps[4] = 1152;    /* screen 3–4 boundary (free 1147–1229)*/
    gs->sea_gap_count = 5;

    /* Initialise the health/lives/score system */
    gs->hearts         = MAX_HEARTS;
    gs->lives          = DEFAULT_LIVES;
    gs->score          = 0;
    gs->coins_for_heart = 0;

    /*
     * Lazy gamepad initialisation — deferred from SDL_Init on purpose.
     *
     * SDL_InitSubSystem activates one SDL subsystem after the main SDL_Init
     * call.  We do this here, after the window is already visible, instead of
     * at process startup.  Antivirus software applies stricter heuristics to
     * code that enumerates HID / XInput devices during the very first frames
     * of a new process; waiting until the game window exists sidesteps those
     * false-positive triggers.
     *
     * Non-fatal: if the subsystem fails to start (e.g. driver issue) the game
     * continues with keyboard-only input.
     */
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Warning: SDL_INIT_GAMECONTROLLER failed: %s — "
                        "gamepad support unavailable\n", SDL_GetError());
    } else {
        /*
         * Scan for already-connected controllers.
         * SDL_IsGameController checks whether SDL has a known mapping for
         * the device (DualSense, DS4, Xbox, etc.).  We open the first one
         * found; hot-plug events (SDL_CONTROLLERDEVICEADDED) handle
         * controllers connected after this point.
         */
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                gs->controller = SDL_GameControllerOpen(i);
                if (gs->controller)
                    break;
            }
        }
    }

    /* Signal the loop to start running; game starts in the foreground */
    gs->running = 1;
    gs->paused  = 0;
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

/* ------------------------------------------------------------------ */

/*
 * level_reset — centralised "player died, restart level" handler.
 *
 * Resets every entity array and the player to their initial state.
 * Called from every hearts<=0 branch so all sources of death produce
 * an identical reset — no entity is accidentally left in a stale state.
 *
 * fp_prev_riding is passed by pointer because it lives as a local
 * variable inside game_loop; resetting it here keeps the float-platform
 * stay-on logic from snapping the player to a platform that no longer
 * exists after the reset.
 */
static void level_reset(GameState *gs, int *fp_prev_riding) {
    player_reset(&gs->player);
    coins_init(gs->coins, &gs->coin_count);
    spiders_init(gs->spiders, &gs->spider_count);
    jumping_spiders_init(gs->jumping_spiders, &gs->jumping_spider_count);
    birds_init(gs->birds, &gs->bird_count);
    faster_birds_init(gs->faster_birds, &gs->faster_bird_count);
    fish_init(gs->fish, &gs->fish_count);
    spike_blocks_init(gs->spike_blocks, &gs->spike_block_count, gs->rails);
    float_platforms_init(gs->float_platforms, &gs->float_platform_count, gs->rails);
    bridges_init(gs->bridges, &gs->bridge_count);
    *fp_prev_riding = -1;
}

/* ------------------------------------------------------------------ */

/*
 * apply_damage — centralised damage and knockback handler.
 *
 * amount   : hearts to remove.  Pass gs->hearts for an instant-kill
 *            (e.g. sea gap) that bypasses the normal 1-heart decrement.
 *
 * push     : 1 = apply a knockback impulse opposite to the contact
 *            direction; 0 = skip (sea gap fall has no contact surface).
 *
 * src_cx / src_cy : world-space centre of the damage source.
 *            Used only when push=1 and the player is stationary — the
 *            impulse pushes the player away from this point.
 *            Ignored when push=0.
 *
 * Knockback algorithm (mirrors spike_block_push_player):
 *   • Moving player: reverse the velocity vector, scale to SPIKE_PUSH_SPEED,
 *     add SPIKE_PUSH_VY upward so the recoil always lifts the player.
 *   • Stationary player: push horizontally away from (src_cx, src_cy),
 *     same upward component.
 *   • on_ground cleared so the upward impulse is not cancelled immediately
 *     by the floor-snap in player_update.
 *
 * After the impulse, hurt_timer is set to 1.5 s (invincibility window),
 * the hit SFX is played, and hearts are decremented.  If hearts reach
 * zero the lives/game-over cascade runs and level_reset is called.
 */
static void apply_damage(GameState *gs, int *fp_prev_riding,
                         int amount, int push,
                         float src_cx, float src_cy) {
    if (push) {
        float vx  = gs->player.vx;
        float vy  = gs->player.vy;
        float len = sqrtf(vx * vx + vy * vy);
        if (len > 1.0f) {
            gs->player.vx = -(vx / len) * SPIKE_PUSH_SPEED;
            gs->player.vy = -(vy / len) * SPIKE_PUSH_SPEED + SPIKE_PUSH_VY;
        } else {
            float dir = (gs->player.x + gs->player.w * 0.5f >= src_cx) ? 1.0f : -1.0f;
            gs->player.vx = dir * SPIKE_PUSH_SPEED;
            gs->player.vy = SPIKE_PUSH_VY;
        }
        gs->player.on_ground = 0;
    }
    (void)src_cy;   /* reserved for future vertical-push logic */

    gs->player.hurt_timer = 1.5f;
    if (gs->snd_hit) Mix_PlayChannel(-1, gs->snd_hit, 0);

    gs->hearts -= amount;
    if (gs->hearts <= 0) {
        gs->lives--;
        if (gs->lives <= 0) {
            gs->lives           = DEFAULT_LIVES;
            gs->score           = 0;
            gs->coins_for_heart = 0;
            if (gs->debug_mode) debug_log(&gs->debug, "GAME OVER - reset");
        }
        if (gs->debug_mode) debug_log(&gs->debug, "LIFE LOST lives=%d", gs->lives);
        gs->hearts = MAX_HEARTS;
        level_reset(gs, fp_prev_riding);
    }
}

/* ------------------------------------------------------------------ */

void game_loop(GameState *gs) {
    /*
     * frame_ms — how many milliseconds one frame should take at TARGET_FPS.
     * e.g. 1000ms / 60fps ≈ 16ms per frame.
     * Used as a manual frame cap fallback if VSync is not active.
     */
    const Uint32 frame_ms = 1000 / TARGET_FPS;

    /* Record the timestamp at the start of the very first frame */
    Uint64 prev = SDL_GetTicks64();

    /*
     * fp_prev_riding — index of the float platform the player was standing on
     * during the previous frame, or -1 if none.  Persists across loop iterations
     * so player_update can use it for the stay-on check when a platform moves up.
     */
    int fp_prev_riding = -1;

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

        /*
         * Delta-time guard — cap to 100 ms (≈ 10 fps minimum).
         * When the OS moves the window to the background it can pause or
         * throttle the process for hundreds of milliseconds.  Without this
         * cap the first frame after a focus-loss produces a huge dt that
         * sends physics haywire (entities teleport, hurt_timer expires
         * instantly, collisions pile up) and resets the game state.
         * The same spike happens on Windows when the user drags the window,
         * which blocks the event loop for the duration of the drag.
         */
        if (dt > 0.1f) dt = 0.1f;

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

            } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
                /*
                 * A new gamepad was plugged in while the game is running.
                 * Open it only if we don't already have one active.
                 * event.cdevice.which is the joystick device index (not
                 * an instance ID), so it can be passed directly to
                 * SDL_GameControllerOpen.
                 */
                if (!gs->controller) {
                    gs->controller = SDL_GameControllerOpen(event.cdevice.which);
                }

            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                /*
                 * A gamepad was unplugged.  Check whether it is the one we
                 * have open by comparing instance IDs.
                 * SDL_GameControllerGetJoystick + SDL_JoystickInstanceID
                 * retrieves the runtime ID of our open controller so we can
                 * compare it against the event's which field.
                 */
                if (gs->controller) {
                    SDL_Joystick *joy = SDL_GameControllerGetJoystick(gs->controller);
                    if (SDL_JoystickInstanceID(joy) == event.cdevice.which) {
                        SDL_GameControllerClose(gs->controller);
                        gs->controller = NULL;
                    }
                }

            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                /*
                 * Start (Xbox) / Options (DualSense / DS4) → quit.
                 * Mirrors the ESC key behaviour so the player can exit
                 * from the couch without reaching for the keyboard.
                 */
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    gs->running = 0;
                }

            } else if (event.type == SDL_WINDOWEVENT) {
                /*
                 * SDL_WINDOWEVENT carries several sub-types that describe
                 * changes to the OS window state.  We use two of them:
                 *
                 * FOCUS_LOST  — the window moved to the background (user
                 *               switched to another app, minimised, etc.).
                 *               We pause physics and music so the game
                 *               doesn't advance while unattended and the
                 *               audio doesn't play into the background.
                 *
                 * FOCUS_GAINED — the window came back to the foreground.
                 *               We resume music and reset the frame timestamp
                 *               so the first resumed frame gets a normal dt
                 *               instead of a spike from the pause duration.
                 */
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    gs->paused = 1;
                    Mix_PauseMusic();
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    gs->paused = 0;
                    Mix_ResumeMusic();
                    prev = SDL_GetTicks64();
                }
            }
        }

        /* ---- 2. Update ------------------------------------------- */
        /*
         * cam_x is declared here so it is in scope for both the paused and
         * the active paths.  When paused we jump straight to render with the
         * camera frozen at its last position; when active the camera-update
         * block below overwrites it with the new smoothed value.
         */
        int cam_x = (int)gs->camera.x;

        /*
         * Skip all physics and game-logic updates while the window is in the
         * background.  The render step still runs so the last frame stays
         * visible in the taskbar thumbnail and on the screen.
         */
        if (gs->paused) goto render;

        /* Read keyboard and gamepad; set the player's velocity for this frame */
        player_handle_input(&gs->player, gs->snd_jump, gs->controller,
                            gs->vines, gs->vine_count);

        /*
         * Move the player, resolve floor + platform + float-platform +
         * bouncepad collisions.
         * bounce_idx is set to the bouncepad hit this frame, or -1.
         * fp_landed_idx is set to the float platform the player landed on,
         * or -1 if none.  Both must be initialised to -1 before the call.
         */
        int bounce_idx    = -1;
        int fp_landed_idx = -1;
        player_update(&gs->player, dt,
                      gs->platforms, gs->platform_count,
                      gs->float_platforms, gs->float_platform_count,
                      gs->bouncepads, gs->bouncepad_count,
                      gs->vines, gs->vine_count,
                      gs->bridges, gs->bridge_count,
                      gs->sea_gaps, gs->sea_gap_count,
                      &bounce_idx, &fp_landed_idx,
                      fp_prev_riding);

        /*
         * Bouncepad landing response: start the release animation and play
         * the spring sound.  The launch impulse itself was already applied
         * inside player_update so the player is already moving upward.
         */
        if (bounce_idx >= 0) {
            Bouncepad *bp       = &gs->bouncepads[bounce_idx];
            bp->state           = BOUNCE_ACTIVE;
            bp->anim_frame      = 1;   /* start mid-compress → extends to 0 */
            bp->anim_timer_ms   = 0;
            if (gs->snd_spring) Mix_PlayChannel(-1, gs->snd_spring, 0);
            if (gs->debug_mode) debug_log(&gs->debug, "BOUNCE pad[%d]", bounce_idx);
        }
        /*
         * Sea gap collision — instant life loss.
         *
         * Each sea gap is a SEA_GAP_W-wide hole in the floor.  The player
         * falls through into the water below.  Once the player's physics
         * centre drops below FLOOR_Y + 16 (the water surface) inside any
         * gap, they lose a life (not a heart) and respawn.  This check
         * ignores the invincibility timer — falling into the sea is fatal.
         */
        {
            float pcx = gs->player.x + gs->player.w / 2.0f;
            float pcy = gs->player.y + gs->player.h / 2.0f;
            for (int g = 0; g < gs->sea_gap_count; g++) {
                float gx = (float)gs->sea_gaps[g];
                if (pcx >= gx && pcx < gx + (float)SEA_GAP_W &&
                    pcy > (float)(GAME_H - WATER_ART_H)) {
                    if (gs->debug_mode) debug_log(&gs->debug, "HIT sea gap[%d]", g);
                    /* Sea gap is always lethal — drain all remaining hearts, no push */
                    apply_damage(gs, &fp_prev_riding, gs->hearts, 0, 0.0f, 0.0f);
                    break;
                }
            }
        }

        /* Move spiders along their patrol paths and advance their animation */
        spiders_update(gs->spiders, gs->spider_count, dt,
                       gs->sea_gaps, gs->sea_gap_count);
        /* Move jumping spiders: patrol + periodic jump arcs */
        jumping_spiders_update(gs->jumping_spiders, gs->jumping_spider_count, dt,
                               gs->sea_gaps, gs->sea_gap_count);
        /* Move birds along their sine-wave patrol paths */
        birds_update(gs->birds, gs->bird_count, dt);
        faster_birds_update(gs->faster_birds, gs->faster_bird_count, dt);
        /* Move fish through the water lane and trigger random jump arcs */
        fish_update(gs->fish, gs->fish_count, dt);
        /* Advance each spike block along its rail path (cam_x needed for
         * the waiting-until-visible check on fall-off rails) */
        spike_blocks_update(gs->spike_blocks, gs->spike_block_count, dt, cam_x);

        /*
         * Advance float platforms: drives crumble timers and rail movement.
         * fp_landed_idx tells which platform (if any) the player is standing
         * on this frame, so the crumble timer only accumulates while the
         * player is in contact.
         */
        float_platforms_update(gs->float_platforms, gs->float_platform_count,
                               dt, fp_landed_idx);

        /*
         * Rail-platform player nudge — push the player sideways by the
         * distance the rail platform moved this frame.
         *
         * float_platforms_update has already advanced fp->x to the new
         * position and saved the old position in fp->prev_x, so the delta
         * (fp->x − fp->prev_x) is the exact horizontal displacement for
         * this frame.  Applying it to the player keeps them riding smoothly
         * as the platform orbits the rail loop.
         */
        if (fp_landed_idx >= 0) {
            const FloatPlatform *fp = &gs->float_platforms[fp_landed_idx];
            if (fp->mode == FLOAT_PLATFORM_RAIL) {
                gs->player.x += fp->x - fp->prev_x;
            }
        }

        /*
         * Update fp_prev_riding so next frame's player_update knows which
         * float platform (if any) the player is currently standing on.
         * Reset to -1 whenever the game resets so the stay-on check doesn't
         * reference a stale index after a respawn.
         */
        fp_prev_riding = fp_landed_idx;

        /*
         * Bridge landing check — detect if the player is standing on any
         * active bridge using the same physics-bottom logic as platforms.
         * The bridge_landed_idx drives the crumble timer in bridges_update.
         */
        int bridge_landed_idx = -1;
        if (gs->player.on_ground) {
            float pcx = gs->player.x + gs->player.w / 2.0f;
            SDL_Rect phit = player_get_hitbox(&gs->player);
            int player_bottom = phit.y + phit.h;
            for (int i = 0; i < gs->bridge_count; i++) {
                const Bridge *br = &gs->bridges[i];
                SDL_Rect brect = bridge_get_rect(br);
                /* Player's bottom is within 4 px of the bridge top,
                 * overlaps horizontally, AND the brick under feet is solid */
                if (player_bottom >= brect.y && player_bottom <= brect.y + 4 &&
                    phit.x + phit.w > brect.x && phit.x < brect.x + brect.w &&
                    bridge_has_solid_at(br, pcx)) {
                    bridge_landed_idx = i;
                    break;
                }
            }
        }
        /* Log the first frame the player touches a bridge brick */
        if (bridge_landed_idx >= 0 && gs->debug_mode) {
            float pcx = gs->player.x + gs->player.w / 2.0f;
            const Bridge *br = &gs->bridges[bridge_landed_idx];
            int idx = (int)((pcx - br->x) / BRIDGE_TILE_W);
            if (idx >= 0 && idx < br->brick_count) {
                const BridgeBrick *bk = &br->bricks[idx];
                if (bk->active && !bk->falling && bk->fall_delay < 0.0f) {
                    debug_log(&gs->debug, "BRIDGE brick[%d] touched", idx);
                }
            }
        }
        bridges_update(gs->bridges, gs->bridge_count, dt, bridge_landed_idx,
                       gs->player.x + gs->player.w / 2.0f);

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
                    (int)s->x + SPIDER_ART_X,
                    FLOOR_Y - SPIDER_ART_H,
                    SPIDER_ART_W,
                    SPIDER_ART_H
                };
                /* SDL_HasIntersection returns SDL_TRUE when the two rects overlap */
                if (SDL_HasIntersection(&phit, &shit)) {
                    if (gs->debug_mode) debug_log(&gs->debug, "HIT spider[%d]", i);
                    float sx = shit.x + shit.w * 0.5f;
                    float sy = shit.y + shit.h * 0.5f;
                    apply_damage(gs, &fp_prev_riding, 1, 1, sx, sy);
                    break;
                }
            }

            /* Jumping spiders use the same damage pattern as regular spiders. */
            if (gs->player.hurt_timer == 0.0f) {
                for (int i = 0; i < gs->jumping_spider_count; i++) {
                    const JumpingSpider *js = &gs->jumping_spiders[i];
                    SDL_Rect jhit = {
                        (int)js->x + JSPIDER_ART_X,
                        FLOOR_Y - JSPIDER_ART_H + (int)js->y,
                        JSPIDER_ART_W,
                        JSPIDER_ART_H
                    };
                    if (SDL_HasIntersection(&phit, &jhit)) {
                        if (gs->debug_mode) debug_log(&gs->debug, "HIT jspider[%d]", i);
                        float sx = jhit.x + jhit.w * 0.5f;
                        float sy = jhit.y + jhit.h * 0.5f;
                        apply_damage(gs, &fp_prev_riding, 1, 1, sx, sy);
                        break;
                    }
                }
            }

            /* Bird collision — slow birds in the sky. */
            if (gs->player.hurt_timer == 0.0f) {
                for (int i = 0; i < gs->bird_count; i++) {
                    SDL_Rect bhit = bird_get_hitbox(&gs->birds[i]);
                    if (SDL_HasIntersection(&phit, &bhit)) {
                        if (gs->debug_mode) debug_log(&gs->debug, "HIT bird[%d]", i);
                        float sx = bhit.x + bhit.w * 0.5f;
                        float sy = bhit.y + bhit.h * 0.5f;
                        apply_damage(gs, &fp_prev_riding, 1, 1, sx, sy);
                        break;
                    }
                }
            }

            /* Faster bird collision. */
            if (gs->player.hurt_timer == 0.0f) {
                for (int i = 0; i < gs->faster_bird_count; i++) {
                    SDL_Rect fbhit = faster_bird_get_hitbox(&gs->faster_birds[i]);
                    if (SDL_HasIntersection(&phit, &fbhit)) {
                        if (gs->debug_mode) debug_log(&gs->debug, "HIT fbird[%d]", i);
                        float sx = fbhit.x + fbhit.w * 0.5f;
                        float sy = fbhit.y + fbhit.h * 0.5f;
                        apply_damage(gs, &fp_prev_riding, 1, 1, sx, sy);
                        break;
                    }
                }
            }

            /* Fish can hurt the player both while swimming and while jumping. */
            if (gs->player.hurt_timer == 0.0f) {
                for (int i = 0; i < gs->fish_count; i++) {
                    SDL_Rect fhit = fish_get_hitbox(&gs->fish[i]);
                    if (SDL_HasIntersection(&phit, &fhit)) {
                        if (gs->debug_mode) debug_log(&gs->debug, "HIT fish[%d]", i);
                        float sx = fhit.x + fhit.w * 0.5f;
                        float sy = fhit.y + fhit.h * 0.5f;
                        apply_damage(gs, &fp_prev_riding, 1, 1, sx, sy);
                        break;
                    }
                }
            }

            /* ---- Spike-block collision ---------------------------------- */
            /*
             * Spike blocks deal the same damage as spiders and fish, but also
             * apply a push impulse opposite to the player's movement direction.
             * The invincibility timer (hurt_timer) from the spider/fish check
             * above is re-used here: if it is still > 0 this block is skipped.
             */
            if (gs->player.hurt_timer == 0.0f) {
                SDL_Rect phit = player_get_hitbox(&gs->player);
                for (int i = 0; i < gs->spike_block_count; i++) {
                    if (!gs->spike_blocks[i].active) continue;
                    SDL_Rect sbhit = spike_block_get_hitbox(&gs->spike_blocks[i]);
                    if (SDL_HasIntersection(&phit, &sbhit)) {
                        if (gs->debug_mode) debug_log(&gs->debug, "HIT spike[%d]", i);
                        float sx = sbhit.x + sbhit.w * 0.5f;
                        float sy = sbhit.y + sbhit.h * 0.5f;
                        apply_damage(gs, &fp_prev_riding, 1, 1, sx, sy);
                        break;
                    }
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
                    if (gs->debug_mode) debug_log(&gs->debug, "COIN [%d] score=%d", i, gs->score);
                    gs->coins_for_heart++;
                    if (gs->coins_for_heart >= COINS_PER_HEART) {
                        gs->coins_for_heart = 0;
                        if (gs->hearts < MAX_HEARTS) {
                            gs->hearts++;
                            if (gs->debug_mode) debug_log(&gs->debug, "HEART +1 hearts=%d", gs->hearts);
                        }
                    }
                }
            }
        }

        /* Advance the water scroll offset */
        water_update(&gs->water, dt);
        /* Advance the fog wave positions and spawn the next wave if it is time */
        fog_update(&gs->fog, dt);
        /* Advance the bouncepad release animation (frame 1 → 0 → back to 2) */
        bouncepads_update(gs->bouncepads, gs->bouncepad_count, (Uint32)(dt * 1000.0f));

        /* ---- Camera update --------------------------------------- */
        /*
         * The camera tries to keep the player centred on screen, offset by a
         * directional lookahead that reveals more terrain ahead of the player.
         *
         * target_x = player_center_in_world - half_screen + lookahead
         *
         * lookahead shifts the viewport in the direction the player faces:
         *   facing right (+) → window peeks further right
         *   facing left  (−) → window peeks further left
         */
        float lookahead = gs->player.facing_left ? -(float)CAM_LOOKAHEAD
                                                 :  (float)CAM_LOOKAHEAD;

        float cam_target = (gs->player.x + gs->player.w * 0.5f)
                           - (GAME_W * 0.5f)
                           + lookahead;

        /*
         * Clamp the target to the world boundaries so the camera never
         * shows the void beyond the left or right edge of the level.
         * When the target is clamped, the camera stops moving and the player
         * walks freely within the screen until facing back toward open space.
         */
        if (cam_target < 0.0f)               cam_target = 0.0f;
        if (cam_target > WORLD_W - GAME_W)   cam_target = (float)(WORLD_W - GAME_W);

        /*
         * Smooth follow: close a fraction of the remaining gap each frame.
         * CAM_SMOOTHING × dt gives the fraction: at 60 fps (dt≈0.016) and
         * CAM_SMOOTHING=8, each frame closes 8×0.016 ≈ 13% of the gap.
         * When the gap is tiny (< CAM_SNAP_THRESHOLD px), snap exactly to
         * prevent endless sub-pixel drift.
         */
        float cam_diff = cam_target - gs->camera.x;
        if (cam_diff > CAM_SNAP_THRESHOLD || cam_diff < -CAM_SNAP_THRESHOLD)
            gs->camera.x += cam_diff * CAM_SMOOTHING * dt;
        else
            gs->camera.x = cam_target;

        /*
         * Convert to integer pixels for this frame's render calls.
         * All world-space render functions subtract cam_x from their dst.x
         * to convert from world coordinates to screen coordinates.
         */
        cam_x = (int)gs->camera.x;

        /* ---- 3. Render ------------------------------------------- */
        render:
        /*
         * Update the debug overlay even while paused so the FPS counter
         * keeps measuring render frames and log entries age correctly.
         */
        if (gs->debug_mode) debug_update(&gs->debug, dt);

        /*
         * SDL_RenderClear — fill the back buffer with the draw colour.
         * We always clear before drawing to avoid leftover pixels from the
         * previous frame showing through.
         */
        SDL_RenderClear(gs->renderer);

        /*
         * Draw the multi-layer parallax background, back-to-front.
         * Each layer scrolls at a fraction of cam_x to simulate depth.
         * cam_x is the integer camera offset computed above.
         */
        parallax_render(&gs->parallax, gs->renderer, cam_x);

        /*
         * Draw the platforms BEFORE the floor so the floor tiles render
         * on top, hiding the 16 px of each pillar that sinks below FLOOR_Y.
         * This makes the pillars look like they grow out of the ground.
         */
        platforms_render(gs->platforms, gs->platform_count,
                         gs->renderer, gs->platform_tex, cam_x);

        /*
         * 9-slice floor rendering — camera-aware, world-wide.
         *
         * The 48×48 Grass_Tileset.png is divided into a 3×3 grid of 16×16 pieces
         * (TILE_SIZE / 3 = 16). Layout:
         *
         *   [TL][TC][TR]   row 0  y= 0..15  ← grass edge
         *   [ML][MC][MR]   row 1  y=16..31  ← dirt interior
         *   [BL][BC][BR]   row 2  y=32..47  ← floor base edge
         *
         * Piece column selection (based on world-space tx):
         *   • tx == 0              → col 0 (left  world edge cap)
         *   • tx + P >= WORLD_W    → col 2 (right world edge cap)
         *   • all other columns    → col 1 (seamless center fill)
         *
         * We iterate tx in world coordinates starting from the tile-aligned
         * column just behind cam_x, and stop once tx is off the right edge
         * of the screen. dst.x = tx - cam_x converts world → screen.
         */
        const int P = TILE_SIZE / 3;   /* 9-slice piece size: 16 px */

        /* First piece column at or before the left edge of the viewport */
        int floor_start_tx = (cam_x / P) * P;

        for (int ty = FLOOR_Y; ty < GAME_H; ty += P) {
            /* Choose which row of the 9-slice to sample */
            int piece_row;
            if (ty == FLOOR_Y)         piece_row = 0;  /* top:    grass edge */
            else if (ty + P >= GAME_H) piece_row = 2;  /* bottom: base edge  */
            else                       piece_row = 1;  /* middle: dirt fill  */

            for (int tx = floor_start_tx; tx < cam_x + GAME_W; tx += P) {
                /*
                 * Skip this piece if it falls inside any sea gap.
                 * A piece at tx is inside a gap when:
                 *   gap_x <= tx  AND  tx + P <= gap_x + SEA_GAP_W
                 * (both edges of the 16-px piece are within the 32-px gap).
                 */
                int in_gap = 0;
                for (int g = 0; g < gs->sea_gap_count; g++) {
                    int gx = gs->sea_gaps[g];
                    if (tx >= gx && tx + P <= gx + SEA_GAP_W) {
                        in_gap = 1;
                        break;
                    }
                }
                if (in_gap) continue;

                /*
                 * Choose which column of the 9-slice to sample.
                 * Use edge caps at gap boundaries so the floor has
                 * clean left/right edges beside each hole.
                 */
                int piece_col;
                int at_left_edge  = 0;
                int at_right_edge = 0;

                /* World boundaries */
                if (tx <= 0)                at_left_edge  = 1;
                if (tx + P >= WORLD_W)      at_right_edge = 1;

                /* Gap boundaries: piece is a right-cap if next piece is gap,
                 * left-cap if previous piece was gap. */
                for (int g = 0; g < gs->sea_gap_count; g++) {
                    int gx = gs->sea_gaps[g];
                    if (tx + P == gx)              at_right_edge = 1;  /* gap starts right after this piece */
                    if (tx     == gx + SEA_GAP_W)  at_left_edge  = 1;  /* gap ends right before this piece  */
                }

                if (at_left_edge && at_right_edge) piece_col = 1;  /* squeezed — use fill */
                else if (at_left_edge)             piece_col = 0;
                else if (at_right_edge)            piece_col = 2;
                else                               piece_col = 1;

                /*
                 * src  — the 16×16 region to cut from the tile sheet.
                 * dst  — world → screen: dst.x = tx - cam_x.
                 *        Pieces outside the viewport are discarded by SDL's
                 *        internal clipping — no manual culling needed.
                 */
                SDL_Rect src = { piece_col * P, piece_row * P, P, P };
                SDL_Rect dst = { tx - cam_x,    ty,            P, P };
                SDL_RenderCopy(gs->renderer, gs->floor_tile, &src, &dst);
            }
        }

        /*
         * Draw floating platforms above the floor and pillar layer but below
         * bouncepads and entities, so they read as mid-air surfaces.
         */
        float_platforms_render(gs->float_platforms, gs->float_platform_count,
                               gs->renderer, gs->float_platform_tex, cam_x);

        /* Draw bridges in the same layer as float platforms */
        bridges_render(gs->bridges, gs->bridge_count,
                       gs->renderer, gs->bridge_tex, cam_x);

        /*
         * Draw bouncepads between the platform pillars and vine decorations.
         * This places them visually on the floor surface, with vines potentially
         * overlapping the edges for a natural overgrown look.
         */
        bouncepads_render(gs->bouncepads, gs->bouncepad_count,
                          gs->renderer, gs->bouncepad_tex, cam_x);
                          
        /*
         * Draw rail tracks before vines and entities so rail tiles appear
         * behind all game objects — the track is part of the background layer.
         */
        if (gs->rail_tex) {
            rail_render(gs->rails, gs->rail_count,
                        gs->renderer, gs->rail_tex, cam_x);
        }

        /* Draw vine decorations on ground and platform tops, behind entities */
        if (gs->vine_tex) {
            vine_render(gs->vines, gs->vine_count,
                        gs->renderer, gs->vine_tex, cam_x);
        }

        /* Draw coins on top of the platforms, before the water and player */
        coins_render(gs->coins, gs->coin_count,
                     gs->renderer, gs->coin_tex, cam_x);

        /* Draw fish behind the water strip (submerged look) but in front of
         * the ground, so the water wave art occludes the submerged portion. */
        fish_render(gs->fish, gs->fish_count,
                gs->renderer, gs->fish_tex, cam_x);

        /*
         * Draw the water strip on top of the floor/platforms and fish.
         * The full 384-px sheet scrolls rightward as a seamless loop.
         */
        water_render(&gs->water, gs->renderer);

        /* Draw spike blocks above the water strip but below the player */
        if (gs->spike_block_tex) {
            spike_blocks_render(gs->spike_blocks, gs->spike_block_count,
                                gs->renderer, gs->spike_block_tex, cam_x);
        }

        /* Draw spiders on top of the water strip, before the player */
        spiders_render(gs->spiders, gs->spider_count,
                       gs->renderer, gs->spider_tex, cam_x);
        /* Draw jumping spiders in the same layer as regular spiders */
        jumping_spiders_render(gs->jumping_spiders, gs->jumping_spider_count,
                               gs->renderer, gs->jumping_spider_tex, cam_x);

        /* Draw birds in the sky, in front of spiders but behind the player */
        birds_render(gs->birds, gs->bird_count,
                     gs->renderer, gs->bird_tex, cam_x);
        faster_birds_render(gs->faster_birds, gs->faster_bird_count,
                            gs->renderer, gs->faster_bird_tex, cam_x);

        /* Draw the player sprite on top of everything */
        player_render(&gs->player, gs->renderer, cam_x);

        /* Draw fog/mist as the topmost layer — rendered after the player */
        fog_render(&gs->fog, gs->renderer);

        /* Draw the HUD overlay on top of everything (hearts, lives, score) */
        hud_render(&gs->hud, gs->renderer, gs->player.texture,
                   gs->hearts, gs->lives, gs->score);

        /* Draw debug overlays (collision boxes, FPS, event log) if active */
        if (gs->debug_mode) {
            debug_render(&gs->debug, gs->hud.font, gs->renderer, gs, cam_x);
        }

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
    /*
     * Close the gamepad and shut down the controller subsystem.
     *
     * SDL_GameControllerClose releases the handle for this specific device.
     * SDL_QuitSubSystem mirrors the SDL_InitSubSystem call in game_init —
     * it decrements the internal reference count for SDL_INIT_GAMECONTROLLER
     * and shuts the subsystem down when the count reaches zero.
     * Both calls are safe when their argument is NULL / the subsystem is
     * not active, so no extra guard is needed beyond the pointer check.
     */
    if (gs->controller) {
        SDL_GameControllerClose(gs->controller);
        gs->controller = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);

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

    if (gs->spike_block_tex) {
        SDL_DestroyTexture(gs->spike_block_tex);
        gs->spike_block_tex = NULL;
    }

    if (gs->bridge_tex) {
        SDL_DestroyTexture(gs->bridge_tex);
        gs->bridge_tex = NULL;
    }

    if (gs->float_platform_tex) {
        SDL_DestroyTexture(gs->float_platform_tex);
        gs->float_platform_tex = NULL;
    }

    if (gs->rail_tex) {
        SDL_DestroyTexture(gs->rail_tex);
        gs->rail_tex = NULL;
    }

    if (gs->vine_tex) {
        SDL_DestroyTexture(gs->vine_tex);
        gs->vine_tex = NULL;
    }

    if (gs->snd_spring) {
        Mix_FreeChunk(gs->snd_spring);
        gs->snd_spring = NULL;
    }

    if (gs->coin_tex) {
        SDL_DestroyTexture(gs->coin_tex);
        gs->coin_tex = NULL;
    }

    if (gs->fish_tex) {
        SDL_DestroyTexture(gs->fish_tex);
        gs->fish_tex = NULL;
    }

    if (gs->faster_bird_tex) {
        SDL_DestroyTexture(gs->faster_bird_tex);
        gs->faster_bird_tex = NULL;
    }

    if (gs->bird_tex) {
        SDL_DestroyTexture(gs->bird_tex);
        gs->bird_tex = NULL;
    }

    if (gs->jumping_spider_tex) {
        SDL_DestroyTexture(gs->jumping_spider_tex);
        gs->jumping_spider_tex = NULL;
    }

    if (gs->spider_tex) {
        SDL_DestroyTexture(gs->spider_tex);
        gs->spider_tex = NULL;
    }

    if (gs->platform_tex) {
        SDL_DestroyTexture(gs->platform_tex);
        gs->platform_tex = NULL;
    }

    if (gs->floor_tile) {
        SDL_DestroyTexture(gs->floor_tile);
        gs->floor_tile = NULL;
    }

    /* Release all parallax layer textures (must precede renderer destruction) */
    parallax_cleanup(&gs->parallax);

    if (gs->renderer) {
        SDL_DestroyRenderer(gs->renderer);
        gs->renderer = NULL;
    }

    if (gs->window) {
        SDL_DestroyWindow(gs->window);
        gs->window = NULL;
    }
}
