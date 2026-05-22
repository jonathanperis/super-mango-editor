/*
 * game_score.h — Shared score and bonus-life helpers.
 */
#pragma once

#include "../game.h"

/* Add score and grant one bonus life for every crossed score_per_life threshold. */
void game_award_score(GameState *gs, int amount);
