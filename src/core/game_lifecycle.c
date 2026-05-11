/*
 * game_lifecycle.c — Game startup and shutdown orchestration.
 */

#include "../game.h"

#include "game_resources.h"
#include "game_window.h"
#include "../effects/fog.h"
#include "../input/game_input.h"
#include "../levels/level_session.h"
#include "../player/player.h"
#include "../screens/hud.h"

/*
 * game_init — Set up every subsystem needed before the game loop starts.
 *
 * Order matters. Window and renderer come first because later resource loads
 * create renderer-owned textures. Level loading happens after HUD/player setup
 * because level_load() configures gameplay state against those live systems.
 */
void game_init(GameState *gs)
{
    game_window_init(gs);

    game_resources_load(gs);

    /* Set up the player (loads texture, sets initial position on the floor). */
    player_init(&gs->player, gs->renderer);

    /* Camera starts at the far-left edge of the world. */
    gs->camera.x = 0.0f;

    /*
     * Fog textures are loaded later, after level_load, because fog texture
     * paths come from the active level definition.
     */

    /* Load HUD font and icon textures while the renderer is available. */
    hud_init(&gs->hud, gs->renderer, gs->textures.star_yellow, gs->player.texture);

    /* Initialise debug overlay if --debug was passed on the CLI. */
    if (gs->debug_mode) debug_init(&gs->debug);

    game_level_load_initial(gs);

    /*
     * Health, lives, and scoring rules are set by level_load() from LevelDef
     * fields, so no hardcoded gameplay defaults belong here.
     */

    gamepad_schedule_deferred_init(gs);

    /* Signal the loop to start running; game starts in the foreground. */
    gs->running = 1;
    gs->paused = 0;
    gs->level_complete = 0;
    gs->checkpoint_x = 0.0f;
}

/*
 * game_cleanup — Free every resource owned by the game.
 *
 * Cleanup mirrors game_init() in reverse. Renderer-dependent resources are
 * destroyed before the renderer, and every helper nulls pointers after free so
 * partial init failure can safely call game_cleanup().
 */
void game_cleanup(GameState *gs)
{
    /* Close gamepad handle, join pending init thread, and quit subsystem. */
    gamepad_cleanup(gs);

    /* Free level storage owned by GameState before renderer-backed assets. */
    game_level_session_cleanup(gs);

    /* Free HUD resources (font + generated textures, renderer-dependent). */
    hud_cleanup(&gs->hud);

    /* Free fog textures before the renderer disappears. */
    fog_cleanup(&gs->fog);

    /* Free player texture before shared texture resources. */
    player_cleanup(&gs->player);

    game_resources_cleanup(gs);

    game_window_cleanup(gs);
}
