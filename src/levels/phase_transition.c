/*
 * phase_transition.c — Pure helpers for level phase transitions.
 */

#include "phase_transition.h"

#include <string.h>

int phase_next_path(const LevelDef *current, char *out, size_t out_size)
{
    if (!current || !out || out_size == 0 || current->next_phase[0] == '\0') {
        return -1;
    }

    strncpy(out, current->next_phase, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
}

int phase_has_next(const LevelDef *current)
{
    return current && current->next_phase[0] != '\0';
}

void phase_progress_save(const GameState *gs, PhaseProgress *progress)
{
    if (!gs || !progress) return;

    progress->score           = gs->score;
    progress->lives           = gs->lives;
    progress->hearts          = gs->hearts;
    progress->score_life_next = gs->score_life_next;
}

void phase_progress_restore(GameState *gs, const PhaseProgress *progress)
{
    if (!gs || !progress) return;

    gs->score           = progress->score;
    gs->lives           = progress->lives;
    gs->hearts          = progress->hearts;
    gs->score_life_next = progress->score_life_next;
}
