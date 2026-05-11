/*
 * game.c — Window, renderer, background, and main game loop.
 */

#include "game.h"
#include "player/player.h"
#include "effects/fog.h"
#include "screens/hud.h"
#include "levels/level_session.h"     /* initial level load                              */
#include "input/game_input.h"          /* gamepad deferred init/cleanup              */
#include "core/game_resources.h"       /* game_resources_load/cleanup                */
#include "core/game_window.h"          /* SDL window and renderer lifecycle           */

/*
 * game_init — Set up everything the game needs before the loop starts.
 *
 * Called once at startup. Creates the OS window, the GPU renderer,
 * loads the background image, and initialises the player.
 */
void game_init(GameState *gs) {
    game_window_init(gs);

    game_resources_load(gs);

    /* Set up the player (loads texture, sets initial position on the floor) */
    player_init(&gs->player, gs->renderer);

    /* Camera starts at the far-left edge of the world. */
    gs->camera.x = 0.0f;

    /*
     * Fog textures are loaded later, after level_load, because the fog
     * texture paths come from the level definition (fog_layers in LevelDef).
     */

    /* Load the HUD font and heart icon texture */
    hud_init(&gs->hud, gs->renderer, gs->textures.star_yellow, gs->player.texture);

    /* Initialise the debug overlay if --debug was passed on the CLI */
    if (gs->debug_mode) debug_init(&gs->debug);

    game_level_load_initial(gs);

    /*
     * Health/lives/score are now set by level_load() from LevelDef fields
     * (initial_hearts, initial_lives, score_per_life).  No hardcoded init
     * needed here — level_load handles it.
     */

    gamepad_schedule_deferred_init(gs);

    /* Signal the loop to start running; game starts in the foreground */
    gs->running = 1;
    gs->paused  = 0;
    gs->level_complete = 0;
    gs->checkpoint_x = 0.0f;
}

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
    gamepad_cleanup(gs);

    game_level_session_cleanup(gs);

    /* Free HUD resources (font + star texture, renderer-dependent) */
    hud_cleanup(&gs->hud);

    /* Free fog textures (renderer-dependent, free before renderer) */
    fog_cleanup(&gs->fog);

    /* Free the player's texture first (also renderer-dependent) */
    player_cleanup(&gs->player);

    game_resources_cleanup(gs);

    game_window_cleanup(gs);
}
