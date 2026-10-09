/*
 * serializer.h — TOML serialization for level definitions.
 *
 * Provides save/load functions that convert between LevelDef structs and
 * TOML files on disk.  Uses the tomlc17 library (vendor/tomlc17) for
 * parsing, and plain fprintf for emitting.
 *
 * The editor uses these to save levels as human-readable .toml files and
 * load them back into the engine's LevelDef format at runtime.
 *
 * Depends on:
 *   - tomlc17 (vendor/tomlc17) for TOML parsing.
 *   - level.h for the LevelDef struct and all placement types.
 */
#pragma once

#include <stddef.h> /* size_t */

#include "../levels/level.h" /* LevelDef and all placement structs */
#include "serializer_io.h"   /* SerializerFileFingerprint */

typedef enum {
    SERIALIZER_SAVE_REPLACE = 0,
    SERIALIZER_SAVE_CREATE_ONLY = 1
} SerializerSavePolicy;

/* ------------------------------------------------------------------ */
/* File I/O                                                            */
/* ------------------------------------------------------------------ */

/*
 * level_save_toml — Serialize a LevelDef to a human-readable TOML file.
 *
 * Writes every field of the LevelDef (name, floor_gaps, all entity arrays,
 * parallax/foreground layers, and level-wide configuration) to the file
 * at `path`.  Enum fields are stored as human-readable strings ("RECT",
 * "SPIN", etc.) so the TOML is easy to read and edit by hand.
 *
 * On POSIX, new files use 0600 permissions; replacement preserves regular-file
 * permissions when available. A sibling temporary file is installed atomically.
 *
 * Returns 0 on success, -1 on error (serialization or file I/O failure).
 */
int level_save_toml(const LevelDef *def, const char *path);

/* Save with explicit create-only or replacement semantics. */
int level_save_toml_with_policy(const LevelDef *def, const char *path,
                                SerializerSavePolicy policy);

/* Save with a final source fingerprint recheck before replacement.
 * Returns -2 (nothing written) when the file no longer matches expected. */
int level_save_toml_checked(const LevelDef *def, const char *path,
                            SerializerSavePolicy policy,
                            const SerializerFileFingerprint *expected);

/*
 * Every save above returns SERIALIZER_REPLACE_TEMP_KEPT (-3) when the
 * operating system moved the old file away but could not put the new one in
 * its place (this can happen on Windows).  The level is then safe in a
 * temporary file next to the destination; this returns that file's path
 * until the next save starts ("" when the last save did not end this way).
 */
const char *level_save_kept_temp_path(void);

/* Save a recovery copy with its known normal destination embedded as metadata. */
int level_save_toml_recovery(const LevelDef *def, const char *path,
                             const char *original_path);

/* Read recovery destination metadata; empty output means destination unknown. */
int level_read_recovery_path(const char *path, char *buf, size_t buf_size);

/*
 * level_load_toml — Read a TOML file and deserialize it into a LevelDef.
 *
 * Opens the file with serializer_fopen_utf8, parses it with
 * toml_parse_file, then walks the resulting table tree to populate `def`.
 * Missing arrays are treated as empty (count = 0).  The result must pass
 * level_validate_runtime; `def` is only written when the whole load
 * succeeds.
 *
 * Returns 0 on success, -1 on error (file not found, parse error, schema
 * error, or a level that fails validation).
 */
int level_load_toml(const char *path, LevelDef *def);
