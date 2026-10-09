/*
 * game_player_step.h — Game-loop player update helper.
 */

#pragma once

#include "../game.h"

/*
 * PlayerStepSupport — which moving or crumbling surface the player stands on
 * after this step's movement and landing tests (-1 = none of that kind).
 * The surfaces' own updates, which run later in the step, read it.
 */
typedef struct {
    int float_platform;   /* float platform landed on, or -1  */
    int bridge;           /* bridge landed on, or -1          */
} PlayerStepSupport;

PlayerStepSupport game_player_step(GameState *gs, float dt);
