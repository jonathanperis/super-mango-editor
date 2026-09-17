/* Opt-in teaching controls. Ordinary gameplay never depends on this state. */
#pragma once
#include "../game.h"

#define INSPECTOR_PHYSICS_COUNT 9

int game_inspector_event(GameState *gs, const SDL_Event *event);
float game_inspector_step(GameState *gs, float elapsed);
void game_inspector_render(GameState *gs);
void game_inspector_cleanup(GameState *gs);
void game_inspector_reset_physics(GameState *gs);
void game_inspector_physics(Player *player, float *values, int apply);
const char *game_inspector_physics_name(int index);
