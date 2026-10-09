/*
 * game_resume.c — Continue points: capture a level part-way, resume it later.
 *
 * Resuming deliberately reuses the life-loss path. reset_current_level puts
 * every enemy, hazard and surface back to its authored start and places the
 * player on (respawn_x, respawn_y), exactly what happens after a lost life;
 * coins collected so far stay collected, as they do after a lost life. So a
 * resumed run plays by the same rules as one that never stopped.
 */
#include "game_resume.h"

#include <string.h>

#include "game_checkpoint.h"
#include "game_state.h"
#include "../levels/level.h"
#include "../shared/platform.h"  /* str_copy */

void game_resume_capture(const GameState *gs, GameResume *resume)
{
    memset(resume, 0, sizeof(*resume));
    str_copy(resume->path, gs->profile_level_key, sizeof(resume->path));
    resume->level_hash = gs->source_level_hash;
    resume->checkpoint = gs->checkpoint_index;
    resume->legacy_screen = gs->legacy_checkpoint_screen;
    resume->respawn_x = gs->respawn_x;
    resume->respawn_y = gs->respawn_y;
    resume->score = gs->score;
    resume->level_score_start = gs->level_score_start;
    resume->score_life_next = gs->score_life_next;
    resume->lives = gs->lives < 0 ? 0 : gs->lives;
    resume->elapsed = gs->completion.level_elapsed;
    /* One bit per coin: bit i set means coins[i] was already collected. */
    for (int i = 0; i < gs->coin_count && i < 64; i++)
        if (!gs->coins[i].active) resume->coins |= (uint64_t)1 << i;
}

/*
 * resume_fits_level — Does the saved respawn point exist in this level?
 *
 * The hash check already proves the file is the same, so these checks only
 * guard against a hand-edited profile: the point must be one the game itself
 * could have saved for this level.
 */
static int resume_fits_level(const GameState *gs, const LevelDef *def, const GameResume *resume)
{
    float start_x, start_y;
    level_effective_spawn(def, &start_x, &start_y);

    /* No coin index the level does not place. */
    for (int i = gs->coin_count; i < 64; i++)
        if (resume->coins & ((uint64_t)1 << i)) return 0;
    if (resume->level_score_start > resume->score) return 0;

    if (resume->checkpoint >= 0) {
        /* An authored checkpoint: the respawn is exactly its x/y. */
        if (resume->checkpoint >= def->checkpoint_count) return 0;
        return resume->respawn_x == def->checkpoints[resume->checkpoint].x &&
               resume->respawn_y == def->checkpoints[resume->checkpoint].y;
    }
    if (def->checkpoint_count > 0) {
        /* Authored checkpoints exist but none was reached: the level start. */
        return resume->respawn_x == start_x && resume->respawn_y == start_y;
    }
    /* Automatic screen checkpoints move only x, along the floor of the
     * screens already entered; y stays the start's. */
    return resume->respawn_y == start_y && resume->respawn_x >= 0.0f &&
           resume->respawn_x < (float)gs->runtime.world_w &&
           resume->legacy_screen <= gs->runtime.world_w / GAME_W;
}

int game_resume_apply(GameState *gs, const GameResume *resume)
{
    const LevelDef *def = (const LevelDef *)gs->runtime.current_level;
    if (!def || !resume || !gs->profile_level_key[0] ||
        strcmp(resume->path, gs->profile_level_key) != 0 ||
        resume->level_hash != gs->source_level_hash ||
        !resume_fits_level(gs, def, resume)) return -1;

    gs->respawn_x = resume->respawn_x;
    gs->respawn_y = resume->respawn_y;
    gs->checkpoint_index = resume->checkpoint;
    gs->legacy_checkpoint_screen = resume->legacy_screen;
    gs->score = resume->score;
    gs->level_score_start = resume->level_score_start;
    gs->score_life_next = resume->score_life_next;
    gs->lives = resume->lives;
    gs->completion.level_elapsed = resume->elapsed;
    for (int i = 0; i < gs->coin_count; i++)
        gs->coins[i].active = (resume->coins & ((uint64_t)1 << i)) == 0;

    /* The same reset a lost life uses: player on the respawn point,
     * enemies and hazards back at their start, camera snapped there. */
    reset_current_level(gs, &gs->loop.fp_prev_riding);
    /* "RESPAWN" on the HUD says where the run picked up again. */
    game_checkpoint_feedback_set(gs, CHECKPOINT_FEEDBACK_RESPAWN,
                                 game_checkpoint_clock_ms(gs), 1200);
    gs->resumed = 1;
    return 0;
}
