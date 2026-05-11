/*
 * player_internal.h — Shared private constants for player implementation files.
 *
 * This header is intentionally not part of the public player API.  It keeps
 * frame and hitbox constants in one place while player.c and extracted helper
 * modules share the same sprite-sheet measurements.
 */
#pragma once

/* Width and height of one sprite frame in the sheet (pixels). */
#define PLAYER_FRAME_W 48
#define PLAYER_FRAME_H 48

/*
 * PLAYER_FLOOR_SINK — pixels the player overlaps with the floor top.
 * Transparent padding at the bottom of the 48×48 frame would otherwise make
 * the character appear to float above the grass.
 */
#define PLAYER_FLOOR_SINK 16

/*
 * Physics-box insets from the 48×48 frame to the visible character art.
 * Pixel analysis: art occupies x=15..32 (18 px), y=18..33 (16 px).
 */
#define PLAYER_PHYS_PAD_X   15
#define PLAYER_PHYS_PAD_TOP 18

/* Coyote time gives a small post-edge jump grace window. */
#define PLAYER_COYOTE_TIME 0.10f

/* Backward-compatible private names used by the remaining monolithic code. */
#define FRAME_W     PLAYER_FRAME_W
#define FRAME_H     PLAYER_FRAME_H
#define FLOOR_SINK  PLAYER_FLOOR_SINK
#define PHYS_PAD_X  PLAYER_PHYS_PAD_X
#define PHYS_PAD_TOP PLAYER_PHYS_PAD_TOP
#define COYOTE_TIME PLAYER_COYOTE_TIME
