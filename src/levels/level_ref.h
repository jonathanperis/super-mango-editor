/*
 * level_ref.h — The one rule for naming a level from data.
 *
 * Three places store a level path as text: [last_star].next_phase inside a
 * level, the campaign manifest's levels array, and the player profile's
 * per-level results.  They must agree, or a phase can load but never record a
 * result.  A "level reference" is therefore always a direct child of levels/:
 *
 *     levels/<name>.toml
 *
 * Subdirectories such as levels/labs/ are standalone examples opened with
 * --level; they are never chained, listed in a campaign, or keyed in a profile.
 */

#pragma once

#include <stddef.h>  /* size_t */

/*
 * level_ref_valid — Return 1 when the first `length` bytes of `ref` form a
 * safe level reference, 0 otherwise.
 *
 * Rejected: missing levels/ prefix or .toml suffix, subdirectories, '\',
 * Windows-reserved characters (<>:"|?*), control bytes including DEL,
 * embedded NUL, an empty stem (".toml", "..toml", ".hidden.toml") and
 * Windows device names such as CON or COM1 (see level_ref.c).
 */
int level_ref_valid(const char *ref, size_t length);
