/*
 * level_validate.h — Helpers shared by the game's level validator
 * (level_validate.c) and the editor's own checks (editor_validation.c).
 *
 * The validator's main entry points, level_validate_counts and
 * level_validate_runtime, are declared in level_loader.h next to the loader
 * that calls them.  This header holds the small rules both programs must
 * apply in exactly the same way, so they are written down once.
 */
#pragma once

#include <stddef.h>   /* size_t */

#include "level.h"    /* LevelDef */

/*
 * level_path_has_parent_segment — 1 when a '/'-separated path contains a
 * ".." segment ("../x.png", "a/../b"), which could climb out of the
 * repository.  "..x" or "x.." are ordinary names and do not count.
 */
int level_path_has_parent_segment(const char *value);

/*
 * level_path_has_control_char — 1 when a path contains a control byte
 * (newline, tab, DEL...).  Such a name cannot be shown or typed reliably
 * and would break the one-path-per-line files the editor writes.
 */
int level_path_has_control_char(const char *value);

/*
 * LevelIssueLocation — where in a level a validation error is, in TOML
 * terms, so a tool can take the designer to it.  It knows nothing about
 * the editor; the editor maps TOML names to its own entity types.
 *
 *   path  : the [[array]] or top-level key: "coins", "screen_count",
 *           "physics", "last_star", "player_start".  "" when unknown.
 *   index : the element of that array, or -1 for a plain key / no element.
 *   field : the key inside it, such as "x" or "tile_height"; "" for none.
 *
 * "coins[3].x is 9000.00 (expected 0.00..1600.00)" reads as
 * { "coins", 3, "x" }; "screen_count is 0 (...)" as { "screen_count", -1, "" }.
 */
typedef struct {
    char path[32];
    int  index;
    char field[32];
} LevelIssueLocation;

/*
 * level_issue_location_parse — Read the location at the start of a
 * validator message (every message begins with its TOML path).  Returns 1
 * and fills *where when the message starts with one, else 0 with an empty
 * location.  Also used for the editor's own "<field> missing: ..." checks.
 */
int level_issue_location_parse(const char *message, LevelIssueLocation *where);

/*
 * level_validate_runtime_at — level_validate_runtime (level_loader.h) plus
 * the location of the first error.  The game keeps calling the plain
 * function; the editor uses this one to make its messages clickable.
 * where may be NULL; on success it is left empty.
 */
int level_validate_runtime_at(const LevelDef *def, char *err, size_t err_size,
                              LevelIssueLocation *where);

/*
 * LevelIssueFn — receives one validation error: its message (the same text
 * level_validate_runtime would write) and where it is.  Both are only valid
 * during the call; copy what you keep.
 */
typedef void (*LevelIssueFn)(void *context, const char *message,
                             const LevelIssueLocation *where);

/*
 * level_validate_runtime_each — Run the same rules as
 * level_validate_runtime, but report every error instead of stopping at
 * the first.  report is called once per error, in the order the checks
 * run, so its first call carries exactly the message the game would show.
 * A rule that depends on an earlier one (a rail rider's t_offset needs a
 * valid rail_index) is skipped once that earlier rule failed, and a bad
 * array count stops everything, since no later check could read the array.
 * Returns how many errors were found (0 = the level is valid).
 */
int level_validate_runtime_each(const LevelDef *def, LevelIssueFn report,
                                void *context);
