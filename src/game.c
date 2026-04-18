/*
 * game.c — Window, renderer, background, and main game loop.
 */

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>    /* TTF_RenderText_Solid — gamepad init HUD message */
#include <stdio.h>
#include <stdlib.h>

#include <string.h> /* strncpy, memset */

#if defined(_WIN32)
#include <windows.h> /* GetFullPathNameA */
#elif !defined(__EMSCRIPTEN__)
#include <limits.h>  /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

#include "game.h"
#include "player/player.h"
#include "surfaces/platform.h"
#include "effects/water.h"
#include "effects/fog.h"
#include "entities/spider.h"
#include "entities/fish.h"
#include "collectibles/coin.h"
#include "surfaces/vine.h"
#include "surfaces/bouncepad.h"
#include "surfaces/bouncepad_small.h"
#include "surfaces/bouncepad_medium.h"
#include "surfaces/bouncepad_high.h"
#include "screens/hud.h"
#include "effects/parallax.h"
#include "surfaces/rail.h"
#include "hazards/spike_block.h"
#include "surfaces/float_platform.h"
#include "collectibles/star_yellow.h"
#include "hazards/axe_trap.h"
#include "hazards/circular_saw.h"
#include "hazards/blue_flame.h"
#include "surfaces/ladder.h"
#include "surfaces/rope.h"
#include "entities/faster_fish.h"
#include "collectibles/last_star.h"
#include "hazards/spike.h"
#include "hazards/spike_platform.h"
#include "levels/level.h"         /* LevelDef struct                                    */
#include "levels/level_loader.h"  /* level_load, level_reset                            */
#include "editor/serializer.h"    /* level_load_toml                                    */
#include "collision/game_collision.h"  /* game_collide, hitbox builders               */
#include "collision/collision_damage.h" /* apply_damage                               */
#include "render/game_render.h"        /* game_render_frame, render overlays         */

/* ------------------------------------------------------------------ */
/* Level data — loaded once from TOML, reused on player death resets    */
/* ------------------------------------------------------------------ */

/*
 * File-scoped level definition.  Populated once from TOML in game_init,
 * then referenced by reset_current_level on player death.
 */
static LevelDef s_level;

/*
 * File-scoped combined bouncepad array — avoids allocating 48 structs
 * on the stack every frame.  Populated once per frame in game_loop_frame.
 */
static Bouncepad s_all_pads[MAX_BOUNCEPADS_MEDIUM + MAX_BOUNCEPADS_SMALL + MAX_BOUNCEPADS_HIGH];

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
     * Load the grass tile. This single 48×48 texture will be repeated
     * (tiled) across the full window width to form the floor.
     */
    gs->floor_tile = IMG_LoadTexture(gs->renderer, "assets/sprites/levels/grass_tileset.png");
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
    gs->platform_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/levels/grass_platform.png");
    if (!gs->platform_tex) {
        fprintf(stderr, "Failed to load Grass_Oneway.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /* Load the animated water strip texture and reset scroll state */
    water_init(&gs->water, gs->renderer);

    /*
     * Load the shared spider texture.  All spider instances blit from
     * this single texture using different source rects per frame.
     */
    gs->spider_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/spider.png");
    if (!gs->spider_tex) {
        fprintf(stderr, "Failed to load Spider_1.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->platform_tex);
        SDL_DestroyTexture(gs->floor_tile);
        parallax_cleanup(&gs->parallax);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * Load the jumping spider texture (Spider_2.png, same layout as Spider_1).
     * Fatal — a gameplay enemy that the player must be able to see.
     */
    gs->jumping_spider_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/jumping_spider.png");
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

    /* ---- Load all entity textures (engine resources) --------------- */
    gs->bird_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/bird.png");
    if (!gs->bird_tex) { fprintf(stderr, "Failed to load Bird_2.png: %s\n", IMG_GetError()); exit(EXIT_FAILURE); }
    gs->faster_bird_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/faster_bird.png");
    if (!gs->faster_bird_tex) { fprintf(stderr, "Failed to load Bird_1.png: %s\n", IMG_GetError()); exit(EXIT_FAILURE); }
    gs->fish_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/fish.png");
    if (!gs->fish_tex) { fprintf(stderr, "Failed to load Fish_2.png: %s\n", IMG_GetError()); exit(EXIT_FAILURE); }
    gs->coin_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/coin.png");
    if (!gs->coin_tex) { fprintf(stderr, "Failed to load Coin.png: %s\n", IMG_GetError()); exit(EXIT_FAILURE); }
    gs->bouncepad_medium_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bouncepad_medium.png");
    if (!gs->bouncepad_medium_tex) { fprintf(stderr, "Failed to load Bouncepad_Wood.png: %s\n", IMG_GetError()); exit(EXIT_FAILURE); }

    /* Non-fatal textures — game runs without them */
    gs->vine_green_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/vine_green.png");
    if (!gs->vine_green_tex) fprintf(stderr, "Warning: Failed to load Vine_Green.png: %s\n", IMG_GetError());
    gs->vine_brown_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/vine_brown.png");
    if (!gs->vine_brown_tex) fprintf(stderr, "Warning: Failed to load Vine_Brown.png: %s\n", IMG_GetError());
    gs->ladder_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/ladder.png");
    if (!gs->ladder_tex) fprintf(stderr, "Warning: Failed to load Ladder.png: %s\n", IMG_GetError());
    gs->rope_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/rope.png");
    if (!gs->rope_tex) fprintf(stderr, "Warning: Failed to load Rope.png: %s\n", IMG_GetError());
    gs->bouncepad_small_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bouncepad_small.png");
    if (!gs->bouncepad_small_tex) fprintf(stderr, "Warning: Failed to load Bouncepad_Green.png: %s\n", IMG_GetError());
    gs->bouncepad_high_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bouncepad_high.png");
    if (!gs->bouncepad_high_tex) fprintf(stderr, "Warning: Failed to load Bouncepad_Red.png: %s\n", IMG_GetError());
    gs->rail_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/rail.png");
    if (!gs->rail_tex) fprintf(stderr, "Warning: Failed to load Rails.png: %s\n", IMG_GetError());
    gs->spike_block_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/spike_block.png");
    if (!gs->spike_block_tex) fprintf(stderr, "Warning: Failed to load Spike_Block.png: %s\n", IMG_GetError());
    gs->float_platform_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/float_platform.png");
    if (!gs->float_platform_tex) fprintf(stderr, "Warning: Failed to load Platform.png: %s\n", IMG_GetError());
    gs->bridge_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/surfaces/bridge.png");
    if (!gs->bridge_tex) fprintf(stderr, "Warning: Failed to load Bridge.png: %s\n", IMG_GetError());
    gs->star_yellow_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/star_yellow.png");
    if (!gs->star_yellow_tex) fprintf(stderr, "Warning: Failed to load star_yellow.png: %s\n", IMG_GetError());
    gs->star_green_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/star_green.png");
    if (!gs->star_green_tex) fprintf(stderr, "Warning: Failed to load star_green.png: %s\n", IMG_GetError());
    gs->star_red_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/star_red.png");
    if (!gs->star_red_tex) fprintf(stderr, "Warning: Failed to load star_red.png: %s\n", IMG_GetError());
    gs->last_star_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/last_star.png");
    if (!gs->last_star_tex) fprintf(stderr, "Warning: Failed to load last_star.png: %s\n", IMG_GetError());
    gs->axe_trap_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/axe_trap.png");
    if (!gs->axe_trap_tex) fprintf(stderr, "Warning: Failed to load Axe_Trap.png: %s\n", IMG_GetError());
    gs->circular_saw_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/circular_saw.png");
    if (!gs->circular_saw_tex) fprintf(stderr, "Warning: Failed to load Circular_Saw.png: %s\n", IMG_GetError());
    gs->blue_flame_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/blue_flame.png");
    if (!gs->blue_flame_tex) fprintf(stderr, "Warning: Failed to load blue_flame.png: %s\n", IMG_GetError());
    gs->fire_flame_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/fire_flame.png");
    if (!gs->fire_flame_tex) fprintf(stderr, "Warning: Failed to load fire_flame.png: %s\n", IMG_GetError());
    gs->faster_fish_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/entities/faster_fish.png");
    if (!gs->faster_fish_tex) fprintf(stderr, "Warning: Failed to load Fish_1.png: %s\n", IMG_GetError());
    gs->spike_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/spike.png");
    if (!gs->spike_tex) fprintf(stderr, "Warning: Failed to load Spike.png: %s\n", IMG_GetError());
    gs->spike_platform_tex = IMG_LoadTexture(gs->renderer, "assets/sprites/hazards/spike_platform.png");
    if (!gs->spike_platform_tex) fprintf(stderr, "Warning: Failed to load Spike_Platform.png: %s\n", IMG_GetError());

    /* ---- Load all sound effects ----------------------------------- */
    gs->snd_spring = Mix_LoadWAV("assets/sounds/surfaces/bouncepad.wav");
    if (!gs->snd_spring) fprintf(stderr, "Warning: Failed to load bouncepad.wav: %s\n", Mix_GetError());

    /*
     * Load the swinging axe sound effect.
     * Mix_LoadWAV handles WAV and, with SDL2_mixer ≥ 2.0, also MP3.
     * Non-fatal: gameplay continues without audio if loading fails.
     */
    gs->snd_axe = Mix_LoadWAV("assets/sounds/hazards/axe_trap.wav");
    if (!gs->snd_axe) {
        fprintf(stderr, "Warning: Failed to load axe_trap.wav: %s\n", Mix_GetError());
    }

    /*
     * Load the bird flapping sound effect.
     * Non-fatal: gameplay continues without audio if loading fails.
     */
    gs->snd_flap = Mix_LoadWAV("assets/sounds/entities/bird.wav");
    if (!gs->snd_flap) {
        fprintf(stderr, "Warning: Failed to load flapping.wav: %s\n", Mix_GetError());
    }

    /*
     * Load the spider sound effect (used by jumping spider attack).
     * Non-fatal: gameplay continues without audio if loading fails.
     */
    gs->snd_spider_attack = Mix_LoadWAV("assets/sounds/entities/spider.wav");
    if (!gs->snd_spider_attack) {
        fprintf(stderr, "Warning: Failed to load spider-attack.mp3: %s\n", Mix_GetError());
    }

    /*
     * Load the dive/splash sound for falling into sea gaps.
     * Non-fatal: gameplay continues without audio if loading fails.
     */
    gs->snd_dive = Mix_LoadWAV("assets/sounds/entities/fish.wav");
    if (!gs->snd_dive) {
        fprintf(stderr, "Warning: Failed to load dive.wav: %s\n", Mix_GetError());
    }

    /*
     * Load the jump sound effect. Mix_LoadWAV decodes the WAV into a
     * Mix_Chunk that can be played on any available mixer channel.
     * Assets path is relative to where the binary is run (repo root).
     */
    gs->snd_jump = Mix_LoadWAV("assets/sounds/player/player_jump.wav");
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
    gs->snd_coin = Mix_LoadWAV("assets/sounds/collectibles/coin.wav");
    if (!gs->snd_coin) {
        fprintf(stderr, "Warning: Failed to load coin.wav: %s\n", Mix_GetError());
    }

    /*
     * Load the player-hit SFX.
     * Non-fatal: if loading fails, gameplay continues without this effect.
     */
    gs->snd_hit = Mix_LoadWAV("assets/sounds/player/player_hit.wav");
    if (!gs->snd_hit) {
        fprintf(stderr, "Warning: Failed to load hit.wav: %s\n", Mix_GetError());
    }

    gs->music = NULL;

    /* Set up the player (loads texture, sets initial position on the floor) */
    player_init(&gs->player, gs->renderer);

    /* Camera starts at the far-left edge of the world. */
    gs->camera.x = 0.0f;

    /*
     * Fog textures are loaded later, after level_load, because the fog
     * texture paths come from the level definition (fog_layers in LevelDef).
     */

    /* Load the HUD font and heart icon texture */
    hud_init(&gs->hud, gs->renderer, gs->star_yellow_tex, gs->player.texture);

    /* Initialise the debug overlay if --debug was passed on the CLI */
    if (gs->debug_mode) debug_init(&gs->debug);

    /* Load level from JSON if a path was provided via --level */
    memset(&s_level, 0, sizeof(s_level));

    /*
     * Validate the level path before opening.  Resolve the path to its
     * canonical form with realpath() so symbolic links and ".." are
     * eliminated, then pass only the resolved path to fopen.
     * This satisfies CodeQL's taint analysis for user-controlled paths.
     */
    char safe_path[512] = {0};
    int path_valid = 0;
    if (gs->level_path[0] != '\0') {
#if defined(__EMSCRIPTEN__)
        /* Emscripten has no realpath — use the path as-is */
        strncpy(safe_path, gs->level_path, sizeof(safe_path) - 1);
        path_valid = 1;
#elif defined(_WIN32)
        char resolved[260];  /* MAX_PATH */
        if (GetFullPathNameA(gs->level_path, 260, resolved, NULL)) {
            strncpy(safe_path, resolved, sizeof(safe_path) - 1);
            path_valid = 1;
        }
#else
        char resolved[PATH_MAX];
        if (realpath(gs->level_path, resolved) != NULL) {
            strncpy(safe_path, resolved, sizeof(safe_path) - 1);
            path_valid = 1;
        }
#endif
    }

    if (path_valid &&
        level_load_toml(safe_path, &s_level) == 0) {
        /* Successfully loaded from the resolved path */
    } else {
        if (gs->level_path[0] != '\0')
            fprintf(stderr, "Warning: could not load %s — starting empty level\n",
                    gs->level_path);
        strncpy(s_level.name, "Untitled", sizeof(s_level.name) - 1);
    }
    level_load(gs, &s_level);

    /* ---- Apply level-wide configuration from the LevelDef ----------- */
    {
        const LevelDef *def = (const LevelDef *)gs->current_level;

        /*
         * Background layers — initialise from level definition if layers are
         * defined; fall back to the hardcoded LAYER_CONFIG table otherwise.
         */
        if (def && def->background_layer_count > 0) {
            /*
             * Extract paths and speeds into flat arrays so we can pass them
             * to parallax_init_from_def.  The anonymous struct in LevelDef
             * stores them interleaved; we need separate pointers.
             */
            char  paths[MAX_BACKGROUND_LAYERS][64];
            float speeds[MAX_BACKGROUND_LAYERS];
            int   n = def->background_layer_count;
            if (n > MAX_BACKGROUND_LAYERS) n = MAX_BACKGROUND_LAYERS;
            for (int i = 0; i < n; i++) {
                SDL_strlcpy(paths[i], def->background_layers[i].path, 64);
                speeds[i] = def->background_layers[i].speed;
            }
            parallax_init_from_def(&gs->parallax, gs->renderer,
                                   (const char (*)[64])paths, speeds, n);
        } else {
            /* No level-defined layers — use the hardcoded defaults */
            parallax_init(&gs->parallax, gs->renderer);
        }

        /*
         * Floor tile — reload from level definition if a custom path is set.
         * This allows different levels to use different floor themes (grass,
         * stone, sand, etc.) without changing engine code.
         */
        if (def && def->floor_tile_path[0] != '\0') {
            if (gs->floor_tile) {
                SDL_DestroyTexture(gs->floor_tile);
                gs->floor_tile = NULL;
            }
            gs->floor_tile = IMG_LoadTexture(gs->renderer, def->floor_tile_path);
            if (!gs->floor_tile) {
                fprintf(stderr, "Warning: failed to load floor tile %s: %s\n",
                        def->floor_tile_path, IMG_GetError());
                /* Fall back to default grass tileset */
                gs->floor_tile = IMG_LoadTexture(gs->renderer,
                                                 "assets/sprites/levels/grass_tileset.png");
            }
        }

        /*
         * Foreground strip texture — the LAST foreground layer is used as
         * the animated strip at the bottom of the screen (water, lava,
         * clouds, etc.).  This is a convention: designers put the strip
         * layer last in the foreground_layers list.
         */
        if (def && def->foreground_layer_count > 0) {
            const char *strip = def->foreground_layers[def->foreground_layer_count - 1].path;
            if (strip[0] != '\0')
                water_reload_texture(&gs->water, gs->renderer, strip);
        }

        /*
         * Fog layers — load fog textures from the level definition.
         * Each level specifies its own fog overlay assets in [[fog_layers]].
         * If no fog layers are defined, the fog system is left uninitialised
         * and fog_enabled stays 0 — no fog renders for this level.
         */
        if (def && def->fog_layer_count > 0) {
            char  fog_paths[MAX_FOG_TEXTURES][64];
            int   n = def->fog_layer_count;
            if (n > MAX_FOG_TEXTURES) n = MAX_FOG_TEXTURES;
            for (int i = 0; i < n; i++) {
                SDL_strlcpy(fog_paths[i], def->fog_layers[i].path, 64);
            }
            fog_init(&gs->fog, gs->renderer,
                     (const char (*)[64])fog_paths, n);
        }

        /*
         * Music — load and play from level definition if a path is specified.
         * Mix_Music streams from disk; Mix_PlayMusic(-1) loops indefinitely.
         * Non-fatal: game runs without music if the file is missing.
         */
        if (def && def->music_path[0] != '\0') {
            gs->music = Mix_LoadMUS(def->music_path);
            if (!gs->music) {
                fprintf(stderr, "Warning: failed to load %s: %s\n",
                        def->music_path, Mix_GetError());
            } else {
                Mix_PlayMusic(gs->music, -1);
                Mix_VolumeMusic(def->music_volume > 0 ? def->music_volume : 13);
            }
        }
    }

    /*
     * Health/lives/score are now set by level_load() from LevelDef fields
     * (initial_hearts, initial_lives, score_per_life).  No hardcoded init
     * needed here — level_load handles it.
     */

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
    /*
     * Gamepad init is deferred to the first frame of the game loop.
     * SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) enumerates HID devices,
     * which can block for 20-30 seconds on Windows when antivirus software
     * is active. Deferring keeps game_init fast so the window appears immediately.
     */
    gs->ctrl_pending_init = 1;

    /* Signal the loop to start running; game starts in the foreground */
    gs->running = 1;
    gs->paused  = 0;
    gs->level_complete = 0;
    gs->checkpoint_x = 0.0f;
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
 * reset_current_level — centralised "player died, restart level" handler.
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
void reset_current_level(GameState *gs, int *fp_prev_riding) {
    /* Apply checkpoint offset to spawn position if set */
    if (gs->checkpoint_x > 0.0f) {
        gs->player.spawn_x = gs->checkpoint_x;
    }
    level_reset(gs, &s_level);
    *fp_prev_riding = -1;
}

/* ------------------------------------------------------------------ */

/*
 * ctrl_init_worker — background thread that calls SDL_InitSubSystem.
 *
 * SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) enumerates HID devices and
 * can block for 20-30 seconds on Windows when antivirus software is active.
 * Running it in a separate thread keeps the main loop responsive so the OS
 * does not show "application not responding" and the game continues drawing.
 *
 * Only SDL_InitSubSystem is called here — SDL gamepad functions
 * (SDL_NumJoysticks, SDL_GameControllerOpen) are not thread-safe and must
 * be called from the main thread after this thread finishes.
 */
static int ctrl_init_worker(void *data) {
    GameState *gs = (GameState *)data;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Warning: SDL_INIT_GAMECONTROLLER failed: %s — "
                        "gamepad support unavailable\n", SDL_GetError());
    }
    /*
     * Atomic write: signal the main thread that the subsystem is ready.
     * volatile ensures the compiler does not cache this value in a register.
     */
    gs->ctrl_init_done = 1;
    return 0;
}

/* ------------------------------------------------------------------ */

/*
 * game_loop_frame — Execute one frame of the game loop.
 *
 * Extracted from game_loop so it can be called either in a blocking
 * while loop (native) or as an emscripten_set_main_loop_arg callback
 * (WebAssembly).  The void* parameter is cast back to GameState*.
 */
static void game_loop_frame(void *arg) {
    GameState *gs = (GameState *)arg;

    const Uint32 frame_ms = 1000 / TARGET_FPS;

    {
        /*
         * Delta time (dt) — seconds elapsed since the previous frame.
         * SDL_GetTicks64() returns milliseconds since SDL was initialised.
         * Dividing by 1000 converts to seconds (a float like 0.016).
         * We multiply velocities by dt so movement is frame-rate independent.
         */
        Uint64 now = SDL_GetTicks64();
        float  dt  = (float)(now - gs->loop_prev_ticks) / 1000.0f;
        gs->loop_prev_ticks = now;

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
                    gs->loop_prev_ticks = SDL_GetTicks64();
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
         * Also skip updates when level is complete (overlay showing).
         */
        if (gs->paused || gs->level_complete) goto render;

        /* Read keyboard and gamepad; set the player's velocity for this frame */
        player_handle_input(&gs->player, gs->snd_jump, gs->controller,
                            gs->vines, gs->vine_count,
                            gs->ladders, gs->ladder_count,
                            gs->ropes, gs->rope_count);

        /*
         * Move the player, resolve floor + platform + float-platform +
         * bouncepad collisions.
         * bounce_idx is set to the bouncepad hit this frame, or -1.
         * fp_landed_idx is set to the float platform the player landed on,
         * or -1 if none.  Both must be initialised to -1 before the call.
         */
        /*
         * Build a combined bouncepad array for player_update collision.
         * The three separate arrays (medium, small, high) are concatenated
         * so player_update sees all pads in one flat array.
         * Uses file-static s_all_pads to avoid stack allocation every frame.
         */
        int all_pad_count = 0;
        for (int i = 0; i < gs->bouncepad_medium_count; i++)
            s_all_pads[all_pad_count++] = gs->bouncepads_medium[i];
        for (int i = 0; i < gs->bouncepad_small_count; i++)
            s_all_pads[all_pad_count++] = gs->bouncepads_small[i];
        for (int i = 0; i < gs->bouncepad_high_count; i++)
            s_all_pads[all_pad_count++] = gs->bouncepads_high[i];

        int bounce_idx    = -1;
        int fp_landed_idx = -1;
        player_update(&gs->player, dt,
                      gs->platforms, gs->platform_count,
                      gs->float_platforms, gs->float_platform_count,
                      s_all_pads, all_pad_count,
                      gs->vines, gs->vine_count,
                      gs->ladders, gs->ladder_count,
                      gs->ropes, gs->rope_count,
                      gs->bridges, gs->bridge_count,
                      gs->spike_platforms, gs->spike_platform_count,
                      gs->floor_gaps, gs->floor_gap_count,
                      &bounce_idx, &fp_landed_idx,
                      gs->fp_prev_riding,
                      gs->world_w);

        /*
         * Bouncepad landing response: start the release animation and play
         * the spring sound.  The launch impulse itself was already applied
         * inside player_update so the player is already moving upward.
         */
        if (bounce_idx >= 0) {
            /*
             * Map the combined-array index back to the correct sub-array.
             * Order: medium (0..mc-1), small (mc..mc+sc-1), high (mc+sc..).
             */
            Bouncepad *bp = NULL;
            int mc = gs->bouncepad_medium_count;
            int sc = gs->bouncepad_small_count;
            if (bounce_idx < mc) {
                bp = &gs->bouncepads_medium[bounce_idx];
            } else if (bounce_idx < mc + sc) {
                bp = &gs->bouncepads_small[bounce_idx - mc];
            } else {
                bp = &gs->bouncepads_high[bounce_idx - mc - sc];
            }
            bp->state           = BOUNCE_ACTIVE;
            bp->anim_frame      = 1;
            bp->anim_timer_ms   = 0;
            if (gs->snd_spring) Mix_PlayChannel(-1, gs->snd_spring, 0);
            if (gs->debug_mode) {
                static const char *pad_names[] = { "GREEN(small)", "WOOD(medium)", "RED(high)" };
                const char *name = pad_names[1]; /* default medium */
                if (bounce_idx < mc) name = pad_names[1];
                else if (bounce_idx < mc + sc) name = pad_names[0];
                else name = pad_names[2];
                debug_log(&gs->debug, "BOUNCE %s", name);
            }
        }
        /*
         * Floor gap collision — instant life loss.
         *
         * Each floor gap is a FLOOR_GAP_W-wide hole in the floor.  The player
         * falls through into the water below.  Once the player's physics
         * centre drops below FLOOR_Y + 16 (the water surface) inside any
         * gap, they lose a life (not a heart) and respawn.  This check
         * ignores the invincibility timer — falling into the gap is fatal.
         */
        {
            float pcx = gs->player.x + gs->player.w / 2.0f;
            float pcy = gs->player.y + gs->player.h / 2.0f;
            for (int g = 0; g < gs->floor_gap_count; g++) {
                float gx = (float)gs->floor_gaps[g];
                if (pcx >= gx && pcx < gx + (float)FLOOR_GAP_W &&
                    pcy > (float)(GAME_H - WATER_ART_H)) {
                    if (gs->debug_mode) debug_log(&gs->debug, "HIT floor gap[%d]", g);
                    /* Play the dive/splash SFX */
                    if (gs->snd_dive) Mix_PlayChannel(-1, gs->snd_dive, 0);
                    /* Floor gap is always lethal — drain all remaining hearts, no push */
                    apply_damage(gs, gs->hearts, 0, 0.0f, 0.0f);
                    break;
                }
            }
        }

        /* Move spiders along their patrol paths and advance their animation */
        spiders_update(gs->spiders, gs->spider_count, dt,
                       gs->floor_gaps, gs->floor_gap_count);
        /* Move jumping spiders: patrol + periodic jump arcs */
        jumping_spiders_update(gs->jumping_spiders, gs->jumping_spider_count, dt,
                               gs->floor_gaps, gs->floor_gap_count,
                               gs->snd_spider_attack,
                               gs->player.x + gs->player.w / 2.0f, cam_x);
        /* Move birds along their sine-wave patrol paths */
        birds_update(gs->birds, gs->bird_count, dt, gs->snd_flap,
                     gs->player.x + gs->player.w / 2.0f, cam_x);
        faster_birds_update(gs->faster_birds, gs->faster_bird_count, dt, gs->snd_flap,
                            gs->player.x + gs->player.w / 2.0f, cam_x);
        /* Move fish through the water lane and trigger random jump arcs */
        fish_update(gs->fish, gs->fish_count, dt, gs->world_w);
        /* Move faster fish through the water lane with higher jumps */
        faster_fish_update(gs->faster_fish, gs->faster_fish_count, dt, gs->world_w);
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
         * Update gs->fp_prev_riding so next frame's player_update knows which
         * float platform (if any) the player is currently standing on.
         * Reset to -1 whenever the game resets so the stay-on check doesn't
         * reference a stale index after a respawn.
         */
        gs->fp_prev_riding = fp_landed_idx;

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

        game_collide(gs, dt);

        /*
         * Checkpoint update — save progress at each screen boundary.
         * When player crosses into a new screen (every GAME_W pixels),
         * update checkpoint_x to the left edge of that screen.
         */
        {
            int current_screen = (int)(gs->player.x / GAME_W);
            float new_checkpoint = current_screen * GAME_W;
            if (new_checkpoint > gs->checkpoint_x) {
                gs->checkpoint_x = new_checkpoint;
                if (gs->debug_mode) {
                    debug_log(&gs->debug, "CHECKPOINT saved at x=%.0f", gs->checkpoint_x);
                }
            }
        }

        /* Advance the water scroll offset (if water is enabled for this level) */
        if (gs->water_enabled) water_update(&gs->water, dt);
        /* Advance the fog wave positions and spawn the next wave if it is time.
         * Only active when the level definition enables fog. */
        if (gs->fog_enabled) fog_update(&gs->fog, dt);
        /* Advance the bouncepad release animation (frame 1 → 0 → back to 2) */
        bouncepads_update(gs->bouncepads_medium, gs->bouncepad_medium_count, (Uint32)(dt * 1000.0f));
        bouncepads_update(gs->bouncepads_small, gs->bouncepad_small_count, (Uint32)(dt * 1000.0f));
        bouncepads_update(gs->bouncepads_high, gs->bouncepad_high_count, (Uint32)(dt * 1000.0f));
        /* Advance axe trap swing/spin animation and trigger distance-scaled sound */
        axe_traps_update(gs->axe_traps, gs->axe_trap_count, dt, gs->snd_axe,
                         gs->player.x + gs->player.w / 2.0f, cam_x);
        /* Advance circular saw patrol and spin animation */
        circular_saws_update(gs->circular_saws, gs->circular_saw_count, dt);
        /* Advance blue flame eruption cycles (rise, flip, fall, wait) */
        blue_flames_update(gs->blue_flames, gs->blue_flame_count, dt);
        /* Advance fire flame eruption cycles (same mechanics, fire variant) */
        blue_flames_update(gs->fire_flames, gs->fire_flame_count, dt);

        /* ---- Camera update --------------------------------------- */
        /*
         * Velocity-driven lookahead: the camera shifts ahead of the player
         * proportionally to their current horizontal velocity (vx).
         *
         *   lookahead = vx × CAM_LOOKAHEAD_VX_FACTOR
         *
         * This means:
         *   vx = 0  → lookahead = 0  → player perfectly centred.
         *   vx = max run speed       → lookahead ≈ CAM_LOOKAHEAD_MAX,
         *                              revealing more terrain ahead.
         *   reversing direction      → lookahead crosses 0 and builds in the
         *                              new direction as speed increases again.
         *
         * CAM_LOOKAHEAD_MAX caps the offset so it never grows unboundedly.
         */
        /* Use level-defined camera params if set, otherwise fall back to macros. */
        const LevelDef *cam_def = (const LevelDef *)gs->current_level;
        float cam_vx_factor = (cam_def && cam_def->physics.cam_lookahead_vx_factor != 0.0f)
                                ? cam_def->physics.cam_lookahead_vx_factor
                                : CAM_LOOKAHEAD_VX_FACTOR;
        float cam_max = (cam_def && cam_def->physics.cam_lookahead_max != 0.0f)
                          ? cam_def->physics.cam_lookahead_max
                          : CAM_LOOKAHEAD_MAX;

        float lookahead = gs->player.vx * cam_vx_factor;
        if (lookahead >  cam_max) lookahead =  cam_max;
        if (lookahead < -cam_max) lookahead = -cam_max;

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
        if (cam_target > gs->world_w - GAME_W) cam_target = (float)(gs->world_w - GAME_W);

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
        game_render_frame(gs, cam_x, dt);

        /*
         * Gamepad init state machine — runs non-blocking across multiple frames.
         *
         * State 1: first frame just presented — spawn the background thread.
         * State 2: thread running — check each frame if it has finished.
         *          When done, open the controller on the main thread (thread-safe)
         *          and clear ctrl_pending_init so the HUD message disappears.
         */
        if (gs->ctrl_pending_init == 1) {
            gs->ctrl_init_done   = 0;
            gs->ctrl_init_thread = SDL_CreateThread(ctrl_init_worker, "ctrl_init", gs);
            if (!gs->ctrl_init_thread) {
                fprintf(stderr, "Warning: could not start gamepad init thread: %s\n",
                        SDL_GetError());
                gs->ctrl_pending_init = 0;  /* give up gracefully */
            } else {
                gs->ctrl_pending_init = 2;
                /*
                 * Pre-render the init HUD message once at state 1→2.
                 * Storing it in ctrl_init_msg_tex avoids TTF rasterisation and a
                 * GPU texture upload on every frame for the ~100–200 ms window
                 * while the gamepad subsystem initialises in the background.
                 */
                if (gs->hud.font) {
                    SDL_Color white = {255, 255, 255, 200};
                    SDL_Surface *surf = TTF_RenderText_Solid(
                        gs->hud.font, "A inicializar controle...", white);
                    if (surf) {
                        gs->ctrl_init_msg_tex =
                            SDL_CreateTextureFromSurface(gs->renderer, surf);
                        SDL_FreeSurface(surf);
                    }
                }
            }
        } else if (gs->ctrl_pending_init == 2 && gs->ctrl_init_done) {
            /*
             * SDL_WaitThread — join the thread and free its resources.
             * Safe to call here because ctrl_init_done guarantees the thread
             * has already returned.
             */
            SDL_WaitThread(gs->ctrl_init_thread, NULL);
            gs->ctrl_init_thread = NULL;

            /* Open the first available gamepad on the main thread. */
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    gs->controller = SDL_GameControllerOpen(i);
                    if (gs->controller) break;
                }
            }
            gs->ctrl_pending_init = 0;
            /* Release the cached HUD message texture — no longer needed. */
            if (gs->ctrl_init_msg_tex) {
                SDL_DestroyTexture(gs->ctrl_init_msg_tex);
                gs->ctrl_init_msg_tex = NULL;
            }
        }

        /*
         * Manual frame cap fallback: if the frame finished before frame_ms ms
         * (e.g. VSync is off), sleep for the remaining time.
         * SDL_Delay sleeps the CPU so we don't burn 100% for no reason.
         */
        /*
         * Manual frame cap fallback (native only — Emscripten controls timing).
         */
#ifndef __EMSCRIPTEN__
        Uint64 elapsed = SDL_GetTicks64() - now;
        if (elapsed < frame_ms) {
            SDL_Delay((Uint32)(frame_ms - elapsed));
        }
#endif
    }
}

/* ------------------------------------------------------------------ */

/*
 * game_loop — Run the game until gs->running becomes 0.
 *
 * On native platforms this is a blocking while loop.
 * On Emscripten (WebAssembly) we hand control to the browser's
 * requestAnimationFrame via emscripten_set_main_loop_arg, which
 * calls game_loop_frame once per vsync.
 */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void game_loop(GameState *gs) {
    gs->loop_prev_ticks = SDL_GetTicks64();
    gs->fp_prev_riding  = -1;

#ifdef __EMSCRIPTEN__
    /*
     * emscripten_set_main_loop_arg — register a per-frame callback.
     *   arg 1: callback function (receives void* user data)
     *   arg 2: user data pointer (our GameState)
     *   arg 3: target FPS (0 = use requestAnimationFrame, recommended)
     *   arg 4: simulate_infinite_loop (1 = never return from this call)
     */
    emscripten_set_main_loop_arg(game_loop_frame, gs, 0, 1);
#else
    while (gs->running) {
        game_loop_frame(gs);
    }
#endif
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

    /* Sound chunks — FREE_CHUNK is null-safe; sets pointer to NULL after free. */
    FREE_CHUNK(gs->snd_jump);
    FREE_CHUNK(gs->snd_coin);
    FREE_CHUNK(gs->snd_hit);

    water_cleanup(&gs->water);

    /* Transient HUD texture (may already be NULL if init finished before quit). */
    DESTROY_TEX(gs->ctrl_init_msg_tex);

    /* Entity textures — DESTROY_TEX is null-safe; reverse init order. */
    DESTROY_TEX(gs->spike_block_tex);
    DESTROY_TEX(gs->bridge_tex);
    DESTROY_TEX(gs->float_platform_tex);
    DESTROY_TEX(gs->rail_tex);
    DESTROY_TEX(gs->vine_green_tex);
    DESTROY_TEX(gs->vine_brown_tex);
    DESTROY_TEX(gs->ladder_tex);
    DESTROY_TEX(gs->rope_tex);

    FREE_CHUNK(gs->snd_spring);
    DESTROY_TEX(gs->bouncepad_medium_tex);
    DESTROY_TEX(gs->bouncepad_small_tex);
    DESTROY_TEX(gs->bouncepad_high_tex);

    DESTROY_TEX(gs->coin_tex);
    DESTROY_TEX(gs->star_yellow_tex);
    DESTROY_TEX(gs->star_green_tex);
    DESTROY_TEX(gs->star_red_tex);
    DESTROY_TEX(gs->last_star_tex);

    FREE_CHUNK(gs->snd_dive);
    FREE_CHUNK(gs->snd_spider_attack);
    FREE_CHUNK(gs->snd_flap);
    FREE_CHUNK(gs->snd_axe);

    DESTROY_TEX(gs->axe_trap_tex);
    DESTROY_TEX(gs->circular_saw_tex);
    DESTROY_TEX(gs->blue_flame_tex);
    DESTROY_TEX(gs->fire_flame_tex);
    DESTROY_TEX(gs->faster_fish_tex);
    DESTROY_TEX(gs->spike_tex);
    DESTROY_TEX(gs->spike_platform_tex);
    DESTROY_TEX(gs->fish_tex);
    DESTROY_TEX(gs->faster_bird_tex);
    DESTROY_TEX(gs->bird_tex);
    DESTROY_TEX(gs->jumping_spider_tex);
    DESTROY_TEX(gs->spider_tex);
    /* Free per-platform tileset textures */
    for (int i = 0; i < gs->platform_count; i++) {
        if (gs->platforms[i].tex) {
            SDL_DestroyTexture(gs->platforms[i].tex);
            gs->platforms[i].tex = NULL;
        }
    }
    DESTROY_TEX(gs->platform_tex);
    DESTROY_TEX(gs->floor_tile);

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
