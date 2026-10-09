/*
 * level_session.c — Active level load and phase transition helpers.
 */

#include "level_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "level.h"
#include "level_loader.h"
#include "level_path.h"
#include "level_start.h"
#include "level_resources.h"
#include "phase_transition.h"
#include "../core/game_camera.h"      /* game_camera_snap */
#include "../core/game_completion.h"
#include "../core/game_resources.h"
#include "../core/game_experiment.h"
#include "../shared/platform.h"  /* str_copy */
#include "../shared/serializer.h"

/*
 * read_stable_level — Parse, validate and fingerprint a level into the heap.
 *
 * level_load_toml validates the definition, which is the only validation a
 * level load needs; the callers below never repeat it. The copy lives on the
 * heap because a LevelDef is about 16 KB (see LEVEL_DEF_SIZE_BUDGET in
 * level.h). The file is fingerprinted before and after the parse, so the
 * hash an experiment records is the hash of exactly these bytes. Returns an
 * owned LevelDef, or NULL after printing why.
 */
static LevelDef *read_stable_level(const char *path, uint64_t *hash)
{
    SerializerFileFingerprint before, after;
    LevelDef *level = malloc(sizeof(*level));
    if (!level) {
        fprintf(stderr, "Error: Failed to allocate level storage\n");
        return NULL;
    }
    level_def_init_defaults(level);
    if (serializer_fingerprint_utf8(path, &before) != 1 || level_load_toml(path, level) != 0 ||
        serializer_fingerprint_utf8(path, &after) != 1 || !serializer_fingerprint_equal(&before, &after)) {
        fprintf(stderr, "Error: cannot read stable level bytes: %s\n", path);
        free(level);
        return NULL;
    }
    *hash = after.content_hash;
    return level;
}

/*
 * game_level_commit — Make a checked, heap-staged level the active one.
 *
 * Every step that can fail (resolve, parse, validate, required sprites) has
 * already run, so this function only moves pointers and applies data: it
 * cannot fail, and a failed load before it leaves the current level, path
 * and hash untouched. gs takes ownership of staged.
 */
static void game_level_commit(GameState *gs, LevelDef *staged, uint64_t hash)
{
    free(gs->world.level_def);
    gs->world.level_def = staged;
    gs->world.runtime.current_level = staged;
    gs->world.source_level_hash = hash;
    level_apply(gs, staged);
    game_completion_reset_summary(gs);
    level_resources_apply(gs, staged);
}

/*
 * apply_start_request — Put the player where --start-x or
 * --start-checkpoint asked (gs->screen.start_kind; level_start.h has the rules).
 *
 * The point becomes the respawn point, so a lost life comes back here
 * rather than at the level's start, until a later checkpoint is crossed.
 * Checkpoints already behind it count as reached, without the banner.
 * The attempt is marked start_point_run: it skipped part of the level, so
 * its time is no best time, its recording no ghost, and its respawn point
 * no Continue point (app_session.c checks the flag).
 * Returns -1 (after saying why) when the level has no such start.
 */
static int apply_start_request(GameState *gs, const LevelDef *def)
{
    LevelStart request;
    LevelStartPoint point;
    char err[128];

    if (gs->screen.start_kind == LEVEL_START_DEFAULT) return 0;
    request.kind = (LevelStartKind)gs->screen.start_kind;
    request.x = gs->screen.start_x;
    request.checkpoint = gs->screen.start_checkpoint;
    if (level_start_resolve(def, &request, &point, err, sizeof(err)) != 0) {
        fprintf(stderr, "Error: cannot start %s there: %s\n", gs->world.level_path, err);
        return -1;
    }
    gs->world.respawn_x = point.spawn_x;
    gs->world.respawn_y = point.spawn_y;
    gs->world.checkpoint_index = point.checkpoint_index;
    gs->world.player.spawn_x = point.spawn_x;
    gs->world.player.spawn_y = point.spawn_y;
    player_reset(&gs->world.player);
    gs->screen.start_point_run = 1;
    /* Show the start point at once instead of panning from the left edge. */
    game_camera_snap(gs);
    return 0;
}

/*
 * game_level_start_point_respawn_y — Keep a start-point run's respawn on
 * the ground.
 *
 * A level without authored checkpoints saves one at each newly entered
 * screen, and that moves only respawn_x (game_checkpoint_update): on a
 * normal run respawn_y stays the level start's height, and recorded runs
 * depend on that, so it is left alone. A --start-x run starts at the height
 * of one particular column, such as the top of a pillar. Carried to a later
 * column, that height would put the player in mid-air, or level with
 * nothing, inside a taller pillar. So on such a run the height follows the
 * new column, chosen by the same rule as the start point itself.
 */
void game_level_start_point_respawn_y(GameState *gs)
{
    const LevelDef *def = (const LevelDef *)gs->world.runtime.current_level;
    float top;
    if (!gs->screen.start_point_run || !def) return;
    /* The column search skips floor gaps, so the floor is always found. */
    if (level_ground_top_at(def, gs->world.respawn_x, &top)) gs->world.respawn_y = top;
}

int game_level_load_initial(GameState *gs)
{
    char safe_path[GAME_LEVEL_PATH_MAX] = {0};

    if (!gs || gs->world.level_path[0] == '\0') {
        fprintf(stderr, "Error: initial level path is missing\n");
        return -1;
    }

    if (level_resolve_path(gs->world.level_path, safe_path, sizeof(safe_path)) != 0) {
        fprintf(stderr, "Error: could not resolve initial level: %s\n",
                gs->world.level_path);
        return -1;
    }

    uint64_t source_hash;
    LevelDef *loaded = read_stable_level(safe_path, &source_hash);
    if (!loaded) {
        fprintf(stderr, "Error: could not load initial level: %s\n", safe_path);
        return -1;
    }

    /* Parse and required sprites are checked before replacing active storage. */
    if (game_resources_require_level_textures(gs, loaded) != 0) {
        free(loaded);
        return -1;
    }
    game_level_commit(gs, loaded, source_hash);
    return apply_start_request(gs, loaded);
}

int game_load_next_phase(GameState *gs)
{
    const LevelDef *current = (const LevelDef *)gs->world.runtime.current_level;
    char next_path[256] = {0};
    if (phase_next_path(current, next_path, sizeof(next_path)) != 0) return -1;

    PhaseProgress saved_progress;
    phase_progress_save(gs, &saved_progress);

    char safe_path[GAME_LEVEL_PATH_MAX] = {0};
    if (level_resolve_path(next_path, safe_path, sizeof(safe_path)) != 0) {
        fprintf(stderr, "Error: Failed to resolve next phase path: %s\n", next_path);
        return -1;
    }

    uint64_t source_hash;
    LevelDef *next_level = read_stable_level(safe_path, &source_hash);
    if (!next_level) {
        fprintf(stderr, "Error: Failed to load next phase: %s\n", safe_path);
        return -1;
    }
    if (game_resources_require_level_textures(gs, next_level) != 0) {
        free(next_level);
        return -1;
    }

    /* Every failure point is behind us: from here the switch cannot fail,
     * so the current level is only given up once the next one is certain. */
    game_experiment_cleanup(gs);
    str_copy(gs->world.level_path, next_path, sizeof(gs->world.level_path));
    game_level_commit(gs, next_level, source_hash);

    /* Only campaign progress crosses a phase boundary. Old movement, climbing,
     * and support indices refer to the previous level and must not survive. */
    player_reset(&gs->world.player);
    gs->screen.loop.fp_prev_riding = -1;
    /* Show the new level's start at once. Setting the camera to 0 instead
     * would pan from the left edge whenever the start lies further right. */
    game_camera_snap(gs);

    phase_progress_restore(gs, &saved_progress);
    gs->world.level_score_start = gs->world.score;
    gs->screen.profile_completion_recorded = 0;

    gs->screen.completion.complete = 0;

    if (gs->screen.debug_mode) {
        debug_log(&gs->screen.debug, "PHASE TRANSITION to: %s", safe_path);
    }

    return 0;
}

void game_level_session_cleanup(GameState *gs)
{
    if (gs->world.level_def) {
        free(gs->world.level_def);
        gs->world.level_def = NULL;
    }
    gs->world.runtime.current_level = NULL;
}
