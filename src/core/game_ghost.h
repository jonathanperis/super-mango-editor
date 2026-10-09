/*
 * game_ghost.h — Time trial: record a run, race a translucent "ghost" of
 * your best one.
 *
 * Why store positions, not inputs? The simulation advances in fixed 1/60 s
 * steps, so replaying the recorded inputs would also reproduce a run, but
 * only while every enemy, timer and random number behaved exactly the same
 * and only on exactly the same engine version. A ghost does not need any of
 * that: it only has to show where Mango was. So each fixed step records
 * Mango's pixel position and sprite frame, and drawing the ghost at step n
 * is just reading sample n. Nothing is simulated twice.
 *
 * The parts:
 *   game_ghost.c      recording during play, playback and drawing
 *   game_ghost_file.c the text format and where it is stored (a file next
 *                     to the profile natively, localStorage in a browser)
 * AppSession loads a level's best ghost when the level opens and saves the
 * finished run when it beats that ghost (see app_session.c).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../game.h"
#include "game_profile.h"

/* One ghost covers at most five minutes of fixed steps. A longer run is
 * not recorded: at one sample per step the file would pass ~180 KB, and a
 * time trial is about fast runs. */
#define GHOST_MAX_STEPS (5 * 60 * TARGET_FPS)

/* Format of the stored ghost text, written as format_version. */
#define GHOST_FORMAT_VERSION 1

/* Largest ghost text read or written (bytes): every sample, line overhead
 * and the header fit with room to spare. */
#define GHOST_TEXT_MAX (256 * 1024)

/*
 * Positions are stored as unsigned 16-bit numbers with GHOST_COORD_OFFSET
 * added, so x from -1024 to 64511 fits: wider than the widest world
 * (MAX_LEVEL_SCREENS screens of GAME_W) and enough for a jump above the
 * top of the screen.
 */
#define GHOST_COORD_OFFSET 1024

/* GhostSample.cell: the player sheet frame (row * 4 + column) in the low
 * five bits, plus GHOST_CELL_FACING_LEFT when the sprite is mirrored. */
#define GHOST_CELL_FACING_LEFT 32
#define GHOST_CELL_MAX 63
/* The player sheet is a grid of 48x48 frames, four to a row. */
#define GHOST_SHEET_FRAME 48
#define GHOST_SHEET_COLS 4

/* Where Mango was, and how he looked, after one fixed step. */
typedef struct {
    uint16_t x, y;  /* whole pixels + GHOST_COORD_OFFSET */
    uint8_t cell;   /* sprite frame and facing, see above */
} GhostSample;

/* One complete run of one level. */
typedef struct {
    char level[PROFILE_LEVEL_PATH]; /* profile key, levels/<name>.toml      */
    uint64_t level_hash;            /* the level file's content hash        */
    float time;                     /* completion time in seconds           */
    int count;                      /* samples, one per fixed step          */
    GhostSample *samples;           /* owned; NULL when count is 0          */
} GameGhostTrack;

/*
 * GameGhost — owned by a GameState that saves to a personal profile.
 * Debug, smoke and replay runs have none (GameState.screen.ghost stays NULL).
 */
typedef struct GameGhost {
    GameGhostTrack best;   /* the run to race; best.count == 0 means none  */
    GhostSample *run;      /* this attempt, GHOST_MAX_STEPS samples of room */
    int run_count;         /* samples recorded so far                       */
    int step;              /* fixed steps since this attempt began          */
    int recording;         /* 0 once this attempt cannot become a ghost     */
} GameGhost;

/* ---- Recording and playback (game_ghost.c) ------------------------- */

/* Give gs a ghost recorder. Returns -1 when out of memory (no ghost then). */
int game_ghost_begin(GameState *gs);

/* Start a fresh attempt: Retry, or the next level of a campaign. The best
 * run stays loaded. Safe when gs->screen.ghost is NULL. */
void game_ghost_restart(GameState *gs);

/* After every fixed simulation step: record Mango, advance the ghost. */
void game_ghost_step(GameState *gs);

/* The best run's sample to draw for the current step, or NULL when there
 * is nothing to draw: no best run, the Ghost setting is off, or the best
 * run has already reached its star. game_render.c draws it. */
const GhostSample *game_ghost_current(const GameState *gs);

/* Copy this finished attempt into out (owned; free with
 * game_ghost_track_free). Returns -1 when the attempt cannot be a ghost:
 * it continued from a saved point, ran too long, or recorded nothing. */
int game_ghost_take_run(const GameState *gs, GameGhostTrack *out);

/* Make track the run to race (takes ownership of its samples). */
void game_ghost_set_best(GameState *gs, GameGhostTrack *track);

void game_ghost_track_free(GameGhostTrack *track);
void game_ghost_cleanup(GameState *gs);

/* ---- Text format and storage (game_ghost_file.c) -------------------- */

/* Strict parse of ghost text into out (owned on success). -1 on any
 * damage: wrong version, sizes, digits, or a sample count that does not
 * match `steps`. */
int game_ghost_decode(GameGhostTrack *out, const char *text);

/* Write track as text. -1 when it does not fit or is out of range. */
int game_ghost_encode(const GameGhostTrack *track, char *text, size_t capacity);

/* Native ghost file for a level: next to the profile file, named after it,
 * e.g. ".../profile.toml" + "levels/01_lugio_01.toml" ->
 * ".../profile-ghost-01_lugio_01.toml". Returns -1 when it does not fit. */
int game_ghost_file_path(const char *profile_path, const char *level_key,
                         char *out, size_t size);

/* Read and decode the stored ghost of level_key. Returns 1 found (out
 * owned), 0 none stored, -1 unreadable or invalid. */
int game_ghost_load(const GameProfile *profile, const char *level_key, GameGhostTrack *out);

/* Store track as its level's ghost, replacing an older one. 0 or -1. */
int game_ghost_save(const GameProfile *profile, const GameGhostTrack *track);
