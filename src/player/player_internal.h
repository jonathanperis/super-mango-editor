/*
 * player_internal.h — Shared private constants for player implementation files.
 *
 * This header is intentionally not part of the public player API: only the
 * player .c files in src/player (and white-box tests) include it.  The sprite frame
 * and physics-box sizes other modules need (PLAYER_FRAME_W,
 * PLAYER_PHYS_PAD_X...) are public, in player.h.
 */
#pragma once

#include "player.h"  /* PLAYER_FRAME_W/H, PLAYER_PHYS_PAD_X/TOP, PLAYER_FLOOR_SINK */

/* Coyote time gives a small post-edge jump grace window. */
#define PLAYER_COYOTE_TIME 0.10f
