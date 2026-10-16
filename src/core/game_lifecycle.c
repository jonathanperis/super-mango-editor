/*
 * game_lifecycle.c — Game startup and shutdown orchestration.
 */

#include "../game.h"

#include "game_camera.h"
#include "game_state.h"    /* game_seed_random */
#include "game_resources.h"
#include "game_experiment.h"
#include "game_ghost.h"
#include "game_window.h"
#include "../effects/water.h"
#include "../input/game_input.h"
#include "../input/game_replay.h"
#include "../levels/level_resources.h"
#include "../levels/level_session.h"
#include "../player/player.h"
#include "../screens/hud.h"
#include "../mangolib/maudio.h" /* sound_stop_all */

#include <string.h>           /* memset */

/*
 * game_init — Set up every subsystem needed before the game loop starts.
 *
 * Order matters. Window and renderer come first because later resource loads
 * create renderer-owned textures. Level loading happens after HUD/player setup
 * because level_load() configures gameplay state against those live systems.
 */
int game_init(GameState *gs)
{
    if (game_window_init(gs) != 0) goto fail;

    /*
     * The shared sprites and sounds. AppSession copies its loaded set into
     * gs->assets before calling game_init. A GameState started without a
     * session, as the tests do, still has empty slots: it loads a private
     * set and owns it, so game_cleanup will unload that set.
     */
    if (!game_resources_loaded(&gs->assets)) {
        gs->owns_assets = 1;
        if (game_resources_load(&gs->assets) != 0) goto fail;
    }

    /*
     * From here on, files come from gs->cache, the session's asset cache
     * (NULL without a session: each file is then loaded for this game
     * alone). On a Replay every acquire below finds its file still loaded.
     */

    /* The default water strip; a level may swap it (level_resources_apply). */
    if (water_init(&gs->world.water, gs->cache) != 0) goto fail;

    /* Set up the player (gets its texture, sets initial position on the floor). */
    if (player_init(&gs->world.player, gs->cache) != 0) goto fail;

    /* A safe value until the level says where the player starts; the
     * camera is snapped to the real start once the level is loaded. */
    gs->world.camera.x = 0.0f;

    /*
     * Fog textures are loaded later, after level_load, because fog texture
     * paths come from the active level definition.
     */

    /* Get the HUD font and icon textures while the renderer is available. */
    if (hud_init(&gs->screen.hud, gs->cache, gs->assets.textures.star_yellow,
                 gs->world.player.texture) != 0) {
        goto fail;
    }

    /* Initialise debug overlay if --debug was passed on the CLI. */
    if (gs->screen.debug_mode) debug_init(&gs->screen.debug);

    /* Seed before the level loads: fish jump timers draw from the stream. */
    game_seed_random(gs, gs->screen.random_seed);
    if (game_level_load_initial(gs) != 0) goto fail;
    if (game_replay_load(gs) != 0) goto fail;

    /*
     * The level has placed the player at its start. Put the camera there
     * now, instead of leaving it at x = 0 and letting the per-step easing
     * pan across the level on the first frames (visible whenever a level
     * starts further right than half a screen).
     */
    game_camera_snap(gs);

    /*
     * Health, lives, and scoring rules are set by level_load() from LevelDef
     * fields, so no hardcoded gameplay defaults belong here.
     */

    gamepad_refresh_controller(gs);

    /* Signal the loop to start running; game starts in the foreground. */
    gs->screen.running = 1;
    gs->screen.route = GAME_ROUTE_NONE;
    gs->screen.game_over = 0;
    gs->screen.paused = 0;
    gs->screen.pause_reasons = 0;
    gs->screen.completion.complete = 0;
    /* No release latch here: the session arms it once the profile (and its
     * bindings) is attached, together with controls held on the old screen
     * (session_make_game in app_session.c). */
    return 0;

fail:
    game_cleanup(gs);
    return -1;
}

/*
 * game_cleanup — Free every resource owned by the game.
 *
 * Cleanup mirrors game_init() in reverse. Renderer-dependent resources are
 * released before the renderer, and every helper nulls pointers after release
 * so partial init failure can safely call game_cleanup().
 */
void game_cleanup(GameState *gs)
{
    game_ghost_cleanup(gs);
    game_experiment_cleanup(gs);
    game_inspector_cleanup(gs);
    game_replay_cleanup(gs);
    /* Close only this screen's gamepad handle. AppSession owns the subsystem. */
    gamepad_close_controller(gs);

    /* Tear down debug overlay state after the loop has stopped using it. */
    if (gs->screen.debug_mode) {
        debug_cleanup(&gs->screen.debug);
    }

    /* Free level storage owned by GameState before renderer-backed assets. */
    game_level_session_cleanup(gs);

    /* Release the HUD's font and coin icon to the asset cache. */
    hud_cleanup(&gs->screen.hud, gs->cache);

    /* Silence this game's sound effects; their samples may outlive it. */
    sound_stop_all();

    /* The level's own files: music, fog, water strip, floor tileset,
     * platform tiles and background layers. */
    level_resources_cleanup(gs);

    /* Release the player texture before the shared texture resources. */
    player_cleanup(&gs->world.player, gs->cache);

    /* Unload the shared sprites and sounds only if this GameState loaded
     * them; borrowed ones belong to AppSession and stay loaded for the
     * next game. */
    if (gs->owns_assets) game_resources_unload(&gs->assets);
    gs->owns_assets = 0;
    memset(&gs->assets, 0, sizeof(gs->assets));
    /* Every reference is released; forget the borrowed cache as well. It
     * still holds the files, idle, for the session's next game. */
    gs->cache = NULL;

    game_window_cleanup(gs);
}
