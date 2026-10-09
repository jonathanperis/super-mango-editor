/*
 * level_start.h — Where a playtest starts the player, when it is not the
 * level's own start (game flags --start-x / --start-checkpoint, sent by
 * the editor's "Playtest from here").
 *
 * The rules only read a LevelDef, so the editor uses the same code to
 * refuse a start the game would refuse, before launching anything.
 */
#pragma once

#include <stddef.h>   /* size_t */

#include "level.h"    /* LevelDef */

typedef enum {
    LEVEL_START_DEFAULT = 0,     /* the level's own start (player_start) */
    LEVEL_START_AT_X,            /* dropped onto the ground at x         */
    LEVEL_START_AT_CHECKPOINT    /* standing on an authored checkpoint   */
} LevelStartKind;

/* What was asked for. */
typedef struct {
    LevelStartKind kind;
    float x;          /* LEVEL_START_AT_X: the player's centre, world px */
    int   checkpoint; /* LEVEL_START_AT_CHECKPOINT: [[checkpoints]] index */
} LevelStart;

/*
 * Where the player ends up.  spawn_x / spawn_y mean what a level's
 * player_start_x / player_start_y mean: the left edge of the TILE_SIZE
 * column the player stands in, and the top of the surface under its feet.
 * checkpoint_index is the authored checkpoint already behind that point
 * (-1 when there is none), so crossing it again shows no banner and a
 * lost life respawns at the start point until a later one is reached.
 */
typedef struct {
    float spawn_x;
    float spawn_y;
    int   checkpoint_index;
} LevelStartPoint;

/*
 * level_ground_top_at — The highest surface a player dropped into the
 * column at column_x (its left edge) lands on: the ground floor (unless a
 * floor gap is under the column's centre), a bridge under that centre, or
 * a ground pillar or fixed or crumbling float platform that the player's
 * physics box overlaps, the same tests the collision code makes.  Spike
 * platforms and rail riders do not count: one hurts, the other moves
 * away.  Returns 1 and sets *top, or 0 when there is nothing to stand on
 * (a gap with nothing above it).
 */
int level_ground_top_at(const LevelDef *def, float column_x, float *top);

/*
 * level_start_resolve — Turn a request into a start point.  An x outside
 * the world, a checkpoint index the level does not have, or an x with no
 * ground under it fails: returns -1 with a message in err.  The default
 * start is the level's own (level_effective_spawn).  Returns 0 on success.
 */
int level_start_resolve(const LevelDef *def, const LevelStart *start,
                        LevelStartPoint *out, char *err, size_t err_size);
