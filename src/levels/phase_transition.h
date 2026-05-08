/*
 * phase_transition.h — Pure helpers for level phase transitions.
 */
#pragma once

#include <stddef.h>

#include "level.h"
#include "../game.h"

typedef struct {
    int score;
    int lives;
    int hearts;
    int score_life_next;
} PhaseProgress;

int phase_next_path(const LevelDef *current, char *out, size_t out_size);
void phase_progress_save(const GameState *gs, PhaseProgress *progress);
void phase_progress_restore(GameState *gs, const PhaseProgress *progress);
