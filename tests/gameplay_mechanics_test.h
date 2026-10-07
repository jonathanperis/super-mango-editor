/*
 * gameplay_mechanics_test.h — Helpers shared by the files linked into
 * gameplay-mechanics-test (gameplay_mechanics_test.c, game_replay_test.c).
 */
#ifndef MANGO_GAMEPLAY_MECHANICS_TEST_H
#define MANGO_GAMEPLAY_MECHANICS_TEST_H

#include "game.h"

/* game_init a fixture with seed 7; debug turns the overlay on. */
int mechanics_open_level(GameState *gs, const char *path, int debug);
/* Advance n fixed steps holding the given PLAYER_INPUT_* bits. */
void mechanics_step(GameState *gs, unsigned int input, int n);
/* Run n whole frames through game_frame with fixed-length frames.
 * Returns how many were presented. */
int mechanics_frames(GameState *gs, int n);

int replay_scripts_drive_the_game(void);
int replay_scripts_reject_malformed_files(void);

#endif
