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
