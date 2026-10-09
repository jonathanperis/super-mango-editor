/*
 * game_ghost.c — Record this run, play back the best one.
 *
 * game_loop.c calls game_ghost_step after every fixed simulation step. That
 * one call does both jobs: it appends Mango's position to this attempt's
 * recording, and moves the ghost on to the best run's sample for the same
 * step. Both runs started at step 0, so "the same step" means "the same
 * moment of the run": the ghost shows where the best run was exactly this
 * long after the start.
 */
#include "game_ghost.h"

#include <stdlib.h>
#include <string.h>

#include "../shared/platform.h"  /* str_copy */

int game_ghost_begin(GameState *gs)
{
    GameGhost *ghost = calloc(1, sizeof(*ghost));
    if (!ghost) return -1;
    ghost->run = malloc(GHOST_MAX_STEPS * sizeof(*ghost->run));
    if (!ghost->run) {
        free(ghost);
        return -1;
    }
    game_ghost_cleanup(gs);
    gs->ghost = ghost;
    game_ghost_restart(gs);
    return 0;
}

void game_ghost_restart(GameState *gs)
{
    if (!gs->ghost) return;
    gs->ghost->run_count = 0;
    gs->ghost->step = 0;
    gs->ghost->recording = 1;
}

/* Clamp a pixel coordinate into the stored 16-bit range. */
static uint16_t ghost_coord(float value)
{
    /* Converting a float outside int's range is undefined in C, so bring
     * it near the stored range first (!(a > b) also catches NaN). */
    if (!(value > -2.0f * GHOST_COORD_OFFSET)) value = -2.0f * GHOST_COORD_OFFSET;
    if (value > (float)UINT16_MAX) value = (float)UINT16_MAX;
    int pixel = (int)value + GHOST_COORD_OFFSET;  /* (int) as player_render draws */
    if (pixel < 0) pixel = 0;
    if (pixel > UINT16_MAX) pixel = UINT16_MAX;
    return (uint16_t)pixel;
}

void game_ghost_step(GameState *gs)
{
    GameGhost *ghost = gs->ghost;
    if (!ghost) return;
    /* A run continued from a saved point began part-way through the level,
     * so its recording could never be a whole run: stop recording it. */
    if (gs->resumed) ghost->recording = 0;
    if (ghost->recording) {
        if (ghost->run_count == GHOST_MAX_STEPS) {
            ghost->recording = 0;  /* too long to keep; see GHOST_MAX_STEPS */
        } else {
            const Player *p = &gs->player;
            GhostSample *sample = &ghost->run[ghost->run_count++];
            int cell = (p->frame.y / GHOST_SHEET_FRAME) * GHOST_SHEET_COLS + p->frame.x / GHOST_SHEET_FRAME;
            sample->x = ghost_coord(p->x);
            sample->y = ghost_coord(p->y);
            sample->cell = (uint8_t)((cell & (GHOST_CELL_FACING_LEFT - 1)) |
                                     (p->facing_left ? GHOST_CELL_FACING_LEFT : 0));
        }
    }
    ghost->step++;
}

const GhostSample *game_ghost_current(const GameState *gs)
{
    const GameGhost *ghost = gs->ghost;
    if (!ghost || !gs->profile || !gs->profile->data.settings.ghost || ghost->best.count == 0) return NULL;
    /* step counts the steps already simulated; after step n the real Mango
     * shows the result of step n, which the best run recorded as sample
     * n - 1. Before the first step the ghost waits on its first sample. */
    int index = ghost->step > 0 ? ghost->step - 1 : 0;
    if (index >= ghost->best.count) return NULL;  /* it reached the star */
    return &ghost->best.samples[index];
}

int game_ghost_take_run(const GameState *gs, GameGhostTrack *out)
{
    const GameGhost *ghost = gs->ghost;
    memset(out, 0, sizeof(*out));
    if (!ghost || !ghost->recording || gs->resumed || ghost->run_count == 0 ||
        !gs->profile_level_key[0]) return -1;
    out->samples = malloc((size_t)ghost->run_count * sizeof(*out->samples));
    if (!out->samples) return -1;
    memcpy(out->samples, ghost->run, (size_t)ghost->run_count * sizeof(*out->samples));
    out->count = ghost->run_count;
    out->time = gs->completion.elapsed;
    out->level_hash = gs->source_level_hash;
    str_copy(out->level, gs->profile_level_key, sizeof(out->level));
    return 0;
}

void game_ghost_set_best(GameState *gs, GameGhostTrack *track)
{
    if (!gs->ghost) {
        game_ghost_track_free(track);
        return;
    }
    game_ghost_track_free(&gs->ghost->best);
    gs->ghost->best = *track;
    memset(track, 0, sizeof(*track));  /* ownership moved into the ghost */
}

void game_ghost_track_free(GameGhostTrack *track)
{
    if (!track) return;
    free(track->samples);
    memset(track, 0, sizeof(*track));
}

void game_ghost_cleanup(GameState *gs)
{
    if (!gs->ghost) return;
    game_ghost_track_free(&gs->ghost->best);
    free(gs->ghost->run);
    free(gs->ghost);
    gs->ghost = NULL;
}
