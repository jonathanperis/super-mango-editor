/*
 * game_movers.h — The things that are drawn between two simulation steps.
 *
 * The simulation advances in fixed 1/60 s steps, but a display may refresh
 * faster than that. A frame that falls between two steps draws each moving
 * thing part of the way from where it was before the latest step to where
 * it is now (game_timing.h explains why). That needs, for every kind of
 * moving thing, a copy of its position from before the step, and a way to
 * blend the two.
 *
 * Both jobs are the same for every kind; only "which array, which floats"
 * differs. That difference is one row of the s_movers table in
 * game_movers.c. game_movers_note walks the table before each step, and
 * game_movers_shown walks one row when a layer is drawn. Adding a kind
 * that should move smoothly is one row there, one array in GameMoverPrev
 * (game.h), and one call in its render layer (step 7 of the new-enemy
 * checklist above s_update_steps in game_update.c). A kind that is NOT in the
 * table moves in visible 60 Hz jumps next to smooth neighbours, which is
 * how the axe traps, the flames and the falling bridge bricks used to
 * look.
 *
 * Drawing only: the simulation never reads the stored copies, and
 * game_movers_shown never writes the world. Steps, their order and their
 * inputs are unchanged, so runs and replays are too.
 */
#pragma once

#include <stddef.h>       /* size_t */

#include "../game_fwd.h"  /* GameState, used by pointer only */

/* The kinds, in table order. */
typedef enum {
    MOVER_SPIDERS,
    MOVER_JUMPING_SPIDERS,
    MOVER_BIRDS,
    MOVER_FASTER_BIRDS,
    MOVER_FISH,
    MOVER_FASTER_FISH,
    MOVER_SPIKE_BLOCKS,    /* position, and the spin angle       */
    MOVER_FLOAT_PLATFORMS,
    MOVER_CIRCULAR_SAWS,   /* position, and the spin angle       */
    MOVER_AXE_TRAPS,       /* the swing or spin angle            */
    MOVER_BLUE_FLAMES,     /* height, and the flip at the top    */
    MOVER_FIRE_FLAMES,
    MOVER_BRIDGES,         /* each falling brick's drop          */
    MOVER_KIND_COUNT
} GameMoverKind;

/* Copy every mover's blended values as they are now, before the next fixed
 * step moves them. game_timing_note_step calls it. */
void game_movers_note(GameState *gs);

/*
 * game_movers_shown — One kind's array as this frame should draw it.
 *
 * t is the share of a step that has passed since the latest one (the
 * render view's t): 0 draws the positions noted before the step, 1 the
 * positions after it, anything between a blend. shown is the caller's
 * buffer of shown_size bytes, large enough for the kind's whole array
 * (declare `Spider shown[MAX_SPIDERS]` and pass sizeof(shown)).
 *
 * Returns the array to draw and stores its length in *count. At t >= 1
 * (and with nothing to draw, or before the first step) that is the world's
 * own array and shown is left untouched; otherwise it is shown, holding a
 * copy with the blended values put in. A value that changed by more than
 * GAME_INTERP_MAX_JUMP in one step (a respawn) is drawn where it is now,
 * together with the other positions of the same item. Angles blend along
 * the shorter way round, so a spin from 358 to 2 degrees passes through 0.
 */
const void *game_movers_shown(const GameState *gs, GameMoverKind kind, float t,
                              void *shown, size_t shown_size, int *count);

/* The row's name ("spiders"), for tests and messages; NULL when kind is
 * out of range. */
const char *game_mover_name(GameMoverKind kind);
