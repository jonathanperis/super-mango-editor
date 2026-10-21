/*
 * gameplay_mechanics_test.h — Helpers shared by the files linked into
 * gameplay-mechanics-test (gameplay_mechanics_test.c, game_replay_test.c,
 * creature_update_test.c, player_climb_test.c).
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

/* creature_update_test.c: birds_update, bird_variant_update, spiders_update. */
int birds_update_patrols_and_turns(void);
int bird_wings_beat_at_each_variant_rate(void);
int bird_hitbox_rides_the_sine_wave(void);
int bird_variant_uses_its_spec(void);
int spiders_update_patrols_and_animates(void);
int spiders_turn_at_floor_gaps(void);

/* player_climb_test.c: player/player_climb.c and the climbing controls. */
int climb_grab_zones_cover_every_tile(void);
int climb_grab_picks_the_first_overlap(void);
int climb_update_moves_clamps_and_lets_go(void);
int climb_controls_follow_the_input(void);
int climb_a_whole_vine_up_and_down(void);

#endif
